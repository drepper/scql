#define _GNU_SOURCE 1

#include <array>
#include <cassert>
#include <charconv>
#include <csignal>
#include <cstring>
#include <format>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <error.h>
#include <langinfo.h>
#include <termios.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>

#include "scql.hh"
#include "scql-tab.hh"
#include "scql-scan.hh"

using namespace std::literals;


namespace repl {

  namespace {

    // Terminal escape sequences.
    const char dsr[] { '\e', '[', '6', 'n' };
    const char el0[] { '\e', '[', '0', 'K' };
    const char el0nl[] { '\e', '[', '0', 'K', '\n' };
    const char su[] { '\e', '[', '1', 'S' };

    const char quit_cmd[] = "quit";

    // Characters considered as parts of words.
    std::string wordchars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    bool initialized = false;

    int efd = -1;
    int sfd = -1;

    int cur_width = -1;
    int cur_height = -1;

    termios old_tios;
    termios edit_tios;

  } // anonymous namespace


  void init()
  {
    if (! ::isatty(STDIN_FILENO))
      return;

    winsize ws;
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
      return;
    cur_width = ws.ws_col;
    cur_height = ws.ws_row;

    if (::tcgetattr(STDIN_FILENO, &old_tios) != 0)
      return;
    edit_tios = old_tios;
    ::cfmakeraw(&edit_tios);
    edit_tios.c_oflag |= old_tios.c_oflag & OPOST;


    efd = ::epoll_create1(EPOLL_CLOEXEC);
    if (efd == -1) [[unlikely]]
      return;

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLPRI | EPOLLHUP;
    ev.data.fd = STDIN_FILENO;
    if (::epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) != 0) [[unlikely]] {
      ::close(efd);
      return;
    }

    sigset_t mask;
    sigemptyset(&mask);
    for (auto sig : { SIGSEGV, SIGHUP, SIGWINCH, SIGTERM, SIGINT, SIGQUIT })
      sigaddset(&mask, sig);
    sfd = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1) [[unlikely]] {
      ::epoll_ctl(efd, EPOLL_CTL_DEL, STDIN_FILENO, nullptr);
      ::close(efd);
    }

    initialized = true;
  }


  void fini()
  {
    if (! initialized)
      return;

    ::epoll_ctl(efd, EPOLL_CTL_DEL, sfd, nullptr);
    ::close(sfd);
    ::epoll_ctl(efd, EPOLL_CTL_DEL, STDIN_FILENO, nullptr);
    ::close(efd);
  }


  struct input_sm {
    enum struct states {
      initial,
      utf1,
      utf2,
      utf3,
      esc,
      csi,
      dcs,
      osc,
      sos,
      pm,
      apc,
      sos2,
      pm2,
      apc2,
    };
    enum struct parsed {
      parsing,
      ch,
      nl,
      bs,
      del,
      keypad,
      alt_b,
      alt_f,
      home,
      end,
      back,
      forward,
      delword,
      delbol,
      deleol,
      sigint,
      sigquit,
      eol,
      eot,

      cpr,
      csi,
      dcs,
      osc,
      sos,
      pm,
      apc,
      up,
      down,
      right,
      left,
    };

    struct seq {
      std::string prefix;
      std::string suffix;
      bool args;
    };

    input_sm(bool utf8_) : utf8(utf8_) { }

    void add_seq(seq&& s) { seqs.emplace_back(std::move(s)); }

    parsed in(unsigned char c);

  private:
    bool utf8;
    std::vector<seq> seqs { };

    states state = states::initial;
  };


  input_sm::parsed input_sm::in(unsigned char c)
  {
    parsed res = parsed::parsing;

    // Handle control irregardless of the current state.
    switch (c) {
    case '\003':
      state = states::initial;
      return parsed::sigint;
    case '\004':
      state = states::initial;
      return parsed::eot;
    }

    switch (state) {
    case states::initial:
      if (c == '\e')
        state = states::esc;
      else if (c == '\001')
        res = parsed::home;
      else if (c == '\005')
        res = parsed::end;
      else if (c == '\r')
        res = parsed::nl;
      else if (c == '\v')
        res = parsed::deleol;
      else if (c == '\25')
        res = parsed::delbol;
      else if (c == '\177')
        res = parsed::bs;
      else if (c < 0x80)
        res = parsed::ch;
      else if ((c & 0xe0) == 0xc0)
        state = states::utf1;
      else if ((c & 0xf0) == 0xe0)
        state = states::utf2;
      else if ((c & 0xf8) == 0xf0)
        state = states::utf3;
      break;
    case states::utf1:
      if ((c & 0xc0) == 0x80) {
        state = states::initial;
        res = parsed::ch;
      } else
        state = states::initial;
      break;
    case states::utf2:
      if ((c & 0xc0) == 0x80)
        state = states::utf1;
      else
        state = states::initial;
      break;
    case states::utf3:
      if ((c & 0xc0) == 0x80)
        state = states::utf2;
      else
        state = states::initial;
      break;
    case states::esc:
      if (c == '[')
        state = states::csi;
      else if (c == ']')
        state = states::osc;
      else if (c == 'P')
        state = states::dcs;
      else if (c == 'X')
        state = states::sos;
      else if (c == '^')
        state = states::pm;
      else if (c == '_')
        state = states::apc;
      else if (c == '\r') {
        res = parsed::eol;
        state = states::initial;
      } else if (c == '\177') {
        res = parsed::delword;
        state = states::initial;
      } else if (c == 'b') {
        res = parsed::back;
        state = states::initial;
      } else if (c == 'f') {
        res = parsed::forward;
        state = states::initial;
      } else {
        res = parsed::ch;
        state = states::initial;
      }
      break;
    case states::csi:
      if (! isdigit(c) && c != ';') {
        state = states::initial;
        switch (c) {
        case 'A':
          res = parsed::up;
          break;
        case 'B':
          res = parsed::down;
          break;
        case 'C':
          res = parsed::right;
          break;
        case 'D':
          res = parsed::left;
          break;
        case 'F':
          res = parsed::end;
          break;
        case 'H':
          res = parsed::home;
          break;
        case 'R':
          res = parsed::cpr;
          break;
        case '~':
          res = parsed::keypad;
          break;
        default:
          res = parsed::csi;
          break;
        }
      }
      break;
    case states::osc:
      if (! isdigit(c) && c != ';') {
        state = states::initial;
        res = parsed::osc;
      }
      break;
    case states::dcs:
      if (! isdigit(c) && c != ';') {
        state = states::initial;
        res = parsed::dcs;
      }
      break;
    case states::sos:
      if (c == '\e')
        state = states::sos2;
      break;
    case states::pm:
      if (c == '\e')
        state = states::pm2;
      break;
    case states::apc:
      if (c == '\e')
        state = states::apc2;
      break;
    case states::sos2:
      if (c == '\\') {
        state = states::initial;
        res = parsed::sos;
      } else
        state = states::sos;
      break;
    case states::pm2:
      if (c == '\\') {
        state = states::initial;
        res = parsed::pm;
      } else
        state = states::pm;
      break;
    case states::apc2:
      if (c == '\\') {
        state = states::initial;
        res = parsed::apc;
      } else
        state = states::apc;
      break;
    }

    return res;
  }


  input_sm sm(true);


  template<size_t N>
  std::optional<std::array<int,N>> numeric_parms(const char* buf, int total, char suffix)
  {
    std::array<int,N> res;

    int pos = 0;
    for (size_t i = 0; i < N; ++i) {
      auto[endp,ec] = std::from_chars(buf + pos, buf + total, res[i]);
      int newpos = endp - buf;
      assert(newpos != total);
      if (ec != std::errc() || *endp != (i + 1 == N ? suffix : ';')) {
        return std::nullopt;
      }
      pos = newpos + 1;
    }
    assert(pos == total);

    return res;
  }


  // Implementation of the buffer.
  ssize_t wp = 0;
  ssize_t rp = 0;
  ssize_t nread = 0;
  constexpr ssize_t bufsize = 1024;
  char buf[bufsize];

  void input_reset()
  {
    wp = rp = nread = 0;
  }

  void input_inject(char c)
  {
    assert(nread < bufsize);
    buf[nread++] = c;
  }

  void input_inject(const char* s)
  {
    assert(nread + strlen(s) <= bufsize);
    nread = ::stpcpy(buf + nread, s) - buf;
  }


  int target_col = -1;

  int prompt_row = -1;
  int prompt_col = -1;

  int input_start_row = -1;
  int input_start_col = -1;

  std::string res;
  size_t pos = 0;


  std::pair<int,int> string_coords(size_t p)
  {
    int x = 0;
    int y = 0;
    for (size_t i = 0; i < p; ++i)
      if (res[i] == '\n') {
        x = 0;
        ++y;
      } else
        ++x;
    return std::make_pair(x, y);
  }


  std::pair<int,int> coords(size_t p)
  {
    auto[x,y] = string_coords(p);
    return std::make_pair(x + input_start_col, y + input_start_row);
  }


  void goto_xy(int x, int y)
  {
    auto s = std::format("\e[{};{}H", 1 + y, 1 + x);
    auto n [[maybe_unused]] = ::write(STDOUT_FILENO, s.c_str(), s.size());
  }


  void clreol()
  {
    auto n [[maybe_unused]] = ::write(STDOUT_FILENO, el0, sizeof(el0));
  }


  std::pair<int,int> move(size_t p)
  {
    auto[x,y] = coords(p);
    goto_xy(x, y);
    return std::make_pair(x, y);
  }

  auto move()
  {
    return move(pos);
  }


  void redisplay()
  {
    size_t p = pos;
    while (p < res.size()) {
      auto end = res.find('\n', p);
      auto here = (end == std::string::npos ? res.size() : end) - p;
      auto n [[maybe_unused]] = ::write(STDOUT_FILENO, res.data() + p, here);
      if (end == std::string::npos)
        break;
      n = ::write(STDOUT_FILENO, el0nl, sizeof(el0nl));
      p += here + 1;
      for (int j = 0; j < input_start_col; ++j)
        n = ::write(STDOUT_FILENO, " ", 1);
    }
    auto n [[maybe_unused]] = ::write(STDOUT_FILENO, el0, sizeof(el0));
#ifdef SELECT_REDRAW
    auto[x,_] = move();
    target_col = x - input_start_col;
#endif
  }


  void insert(const char* s, size_t n)
  {
    assert(pos <= res.size());
    while (n > 0) {
      auto[_,y] = res.empty() ? std::make_pair(input_start_col, input_start_row) : coords(res.size() - 1);
      auto endp = static_cast<const char*>(memchr(s, '\n', n));
      auto here = endp == nullptr ? n : (endp - buf);
      res.insert(pos, s, here);
#ifdef SELECT_REDRAW
      ::write(STDOUT_FILENO, s, here);
#endif
      pos += here;
      if (endp == nullptr)
        break;
      res.insert(pos, 1, '\n');
#ifdef SELECT_REDRAW
      :auto n [[maybe_unused]] = :write(STDOUT_FILENO, el0nl, sizeof(el0nl));
#endif
      ++pos;
      if (y + 1 == cur_height) {
        input_start_row -= 1;
#ifndef SELECT_REDRAW
        auto nn [[maybe_unused]] = ::write(STDOUT_FILENO, su, sizeof(su));
#endif
      }
#ifdef SELECT_REDRAW
      for (int j = 0; j < input_start_col; ++j)
        auto nn [[maybe_unused]] = ::write(STDOUT_FILENO, " ", 1);
#endif
      s += here + 1;
      n -= here + 1;
    }
#ifdef SELECT_REDRAW
    redisplay();
#endif
    auto[x,_] = move();
    target_col = x - input_start_col;
  }


  void del(size_t n)
  {
    res.erase(pos, n);
    move();
    redisplay();
  }


  auto prev_word()
  {
    assert(! res.empty() && pos > 0);

    auto p = pos;
    if (p >= res.size() || ! wordchars.contains(res[p]) || ! wordchars.contains(res[p - 1])) {
      if (auto last = res.find_last_of(wordchars, p - 1); last == std::string::npos)
        return 0zu;
      else
        p = last;
    }
    if (auto last = res.find_last_not_of(wordchars, p); last == std::string::npos)
      p = 0;
    else
      p = last + 1;

    return p;
  }


#ifndef SELECT_REDRAW
  void redraw_all()
  {
    auto save = pos;
    pos = 0;
    move();
    redisplay();
    pos = save;
    move();
  }
#endif


    void debug(const std::string& s)
    {
      for (int i = 0; i < 10; ++i) {
        goto_xy(0, i);
        clreol();
      }
      goto_xy(0, 0);
      auto nn [[maybe_unused]] = ::write(STDOUT_FILENO, s.data(), s.size());
      move();
    }


  std::string read(const std::string& prompt)
  {
    res.clear();
    pos = 0;

    prompt_row = -1;
    prompt_col = -1;

    target_col = 0;

    input_start_row = -1;
    input_start_col = -1;

    ::tcsetattr(STDIN_FILENO, TCSANOW, &edit_tios);

    // Request the current cursor position.
    auto nn [[maybe_unused]] = ::write(STDOUT_FILENO, dsr, sizeof(dsr));
    bool received_position = false;

    input_reset();

    while (true) {
      epoll_event evs[1];
      auto n = ::epoll_wait(efd, evs, sizeof(evs) / sizeof(evs[0]), -1);
      if (n == 0)
        break;

      if (evs[0].data.fd == STDIN_FILENO) {
        rp = wp;
        nread = ::read(STDIN_FILENO, &buf[rp], sizeof(buf) - rp);
        if (nread <= 0)
          continue;
        nread += rp;
        bool need_redraw = false;
        bool moved = false;
        while (rp < nread) {
          auto lastch = buf[wp++] = buf[rp++];
          auto pres = sm.in(lastch);
          if (pres == input_sm::parsed::parsing)
            continue;

          switch (pres) {
          case input_sm::parsed::eol:
            pos = res.size();
            move();
            goto out;
          case input_sm::parsed::nl:
            // Translate for Unix systems.
            buf[wp - 1] = '\n';
            insert(reinterpret_cast<char*>(buf), wp);
            need_redraw = true;
            break;
          case input_sm::parsed::ch:
            insert(reinterpret_cast<char*>(buf), wp);
            need_redraw = true;
            break;
          case input_sm::parsed::bs:
            if (pos > 0) {
              --pos;
              del(1);
              need_redraw = true;
            }
            break;
          case input_sm::parsed::del:
          handle_del:
            if (res.size() > pos) {
              del(1);
              need_redraw = true;
            }
            break;
          case input_sm::parsed::keypad:
            {
              auto nrs = numeric_parms<1>(reinterpret_cast<const char*>(buf) + 2, wp - 2, '~');

              if (nrs.has_value())
                switch ((*nrs)[0]) {
                case 1:
                  goto handle_home;
                case 3:
                  goto handle_del;
                case 4:
                  goto handle_end;
                }
            }
            break;
          case input_sm::parsed::home:
          handle_home:
            pos = 0;
            move();
            moved = true;
            break;
          case input_sm::parsed::end:
          handle_end:
            pos = res.size();
            move();
            moved = true;
            break;
          case input_sm::parsed::back:
            if (! res.empty() && pos > 0) {
              pos = prev_word();
              auto[x,_] = move();
              moved = true;
              target_col = x - input_start_col;
            }
            break;
          case input_sm::parsed::forward:
            if (! res.empty() && pos < res.size()) {
              if (! wordchars.contains(res[pos])) {
                if (auto next = res.find_first_of(wordchars, pos + 1); next == std::string::npos) {
                  pos = res.size();
                  auto[x,_] = move();
                  target_col = x - input_start_col;
                  break;
                } else
                  pos = next;
              }
              if (auto next = res.find_first_not_of(wordchars, pos); next == std::string::npos)
                pos = res.size();
              else
                pos = next;
              auto[x,_] = move();
              moved = true;
              target_col = x - input_start_col;
            }
            break;
          case input_sm::parsed::delword:
            if (pos > 0) {
              auto oldpos = pos;
              pos = prev_word();
              if (pos != oldpos) {
                del(oldpos - pos);
                need_redraw = true;
              }
            }
            break;
          case input_sm::parsed::delbol:
            if (pos > 0) {
              auto oldpos = pos;
              pos = 0;
              del(oldpos);
              need_redraw = true;
            }
            break;
          case input_sm::parsed::deleol:
            if (pos < res.size()) {
              del(res.size() - pos);
              need_redraw = true;
            }
            break;
          case input_sm::parsed::sigint:
            // XYZ Indicate user interrupt
            res = "";
            need_redraw = true;
            goto out;
          case input_sm::parsed::eot:
            if (res.empty()) {
              input_reset();
              input_inject(quit_cmd);
              input_inject("\e\r");
              continue;
            }
            // XYZ Ignore otherwise?
            break;
          case input_sm::parsed::left:
            if (pos > 0) {
              --pos;
              auto[x,_] = move();
              moved = true;
              target_col = x - input_start_col;
            }
            break;
          case input_sm::parsed::right:
            if (pos < res.size()) {
              ++pos;
              auto[x,_] = move();
              moved = true;
              target_col = x - input_start_col;
            }
            break;
          case input_sm::parsed::up:
            if (auto curline_start = res.rfind('\n', pos); curline_start != std::string::npos && curline_start > 0) {
              ++curline_start;
              auto prevline_start = res.rfind('\n', curline_start - 2);
              if (prevline_start == std::string::npos)
                prevline_start = 0;
              else
                prevline_start += 1;
              if (curline_start - prevline_start >= size_t(target_col))
                pos = prevline_start + target_col;
              else
                pos = curline_start - 1;
              move();
              moved = true;
            }
            break;
          case input_sm::parsed::down:
            if (auto nextline_start = res.find('\n', pos); nextline_start != std::string::npos && nextline_start + 1 < res.size()) {
              ++nextline_start;
              auto nextnextline_start = res.find('\n', nextline_start);
              if (nextnextline_start == std::string::npos)
                nextnextline_start = res.size();
              else
                nextnextline_start += 1;
              if (nextnextline_start - nextline_start >= size_t(target_col))
                pos = nextline_start + target_col;
              else
                pos = nextnextline_start - 1;
              move();
              moved = true;
            }
            break;
          case input_sm::parsed::cpr:
            {
              auto nrs = numeric_parms<2>(reinterpret_cast<const char*>(buf) + 2, wp - 2, 'R');

              if (! received_position) {
                if (nrs.has_value()) {
                  // Finally show the prompt:
                  nn = ::write(STDOUT_FILENO, prompt.c_str(), prompt.size());
                  prompt_row = (*nrs)[0] - 1;
                  prompt_col = (*nrs)[1] - 1;
                  received_position = true;
                }
                nn = ::write(STDOUT_FILENO, dsr, sizeof(dsr));
              } else {
                if (nrs.has_value()) {
                  input_start_row = (*nrs)[0] - 1;
                  input_start_col = (*nrs)[1] - 1;
                } else
                  nn = ::write(STDOUT_FILENO, dsr, sizeof(dsr));
              }
            }
            break;
          default:
            // Ignore for now.
            break;
          }

          wp = 0;
        }

        if (need_redraw) {
          while (true) {
            auto buffer = scql_scan_bytes(res.data(), res.size());
            // XYZ no need to free buffer
            (void) buffer;

            scql::result.reset();

            auto yyres = yyparse();
            if (yyres != 0) {
              auto[x, y] = string_coords(pos);
              if (scql::result && scql::result->fixup(res, pos, x, y))
                continue;
            }

            break;
          }
          debug(scql::result ? scql::result->format() : ""s);

          redraw_all();
          if (moved) {
          }
        }
      } else if (evs[0].data.fd == sfd) {
        signalfd_siginfo ssi;
        if (::read(sfd, &ssi, sizeof(ssi)) == sizeof(ssi)) {
          break;
        }
      }
    }

  out:
    ::tcsetattr(STDIN_FILENO, TCSANOW, &old_tios);

    return res;
  }


} // namespace repl



#include <iostream>

int main()
{
  std::locale::global(std::locale(""));
  if (strcmp("UTF-8", ::nl_langinfo(CODESET)) != 0)
    ::error(EXIT_FAILURE, 0, "locale with UTF-8 encoding needed");

  repl::init();

  while (true) {
    for (int i = 0; i < repl::cur_width; ++i) std::cout << "\u2501";
    std::cout << "\n";

    auto input = repl::read("prompt> ");
    std::cout << "\n";
    if (input == "quit")
      break;

    std::cout << "handle \"" << input << "\"\n";
  }

  repl::fini();
}
