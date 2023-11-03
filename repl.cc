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


namespace repl {

  namespace {

    // Terminal escape sequences.
    const char dsr[] { '\e', '[', '6', 'n' };
    const char el0nl[] { '\e', '[', '0', 'K', '\n' };

    const char quit_cmd[] = "quit";

  } // anonymous namespace


  bool initialized = false;

  int efd = -1;
  int sfd = -1;

  int cur_width = -1;
  int cur_height = -1;

  termios old_tios;
  termios edit_tios;

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
      alt_b,
      alt_f,
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
      else if (c == '\r')
        res = parsed::nl;
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
        case 'R':
          res = parsed::cpr;
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


  std::pair<int,int> coords(size_t p)
  {
    int x = input_start_col;
    int y = input_start_row;
    for (size_t i = 0; i < p; ++i)
      if (res[i] == '\n') {
        x = input_start_col;
        ++y;
      } else
        ++x;
    return std::make_pair(x, y);
  }


  std::pair<int,int> move(size_t p)
  {
    auto[x,y] = coords(p);
    auto s = std::format("\e[{};{}H", 1 + y, 1 + x);
    ::write(STDOUT_FILENO, s.c_str(), s.size());
    return std::make_pair(x, y);
  }

  auto move()
  {
    return move(pos);
  }


  void redisplay(size_t p)
  {
    while (p < res.size()) {
      auto end = res.find('\n', p);
      auto here = (end == std::string::npos ? res.size() : end) - p;
      ::write(STDOUT_FILENO, res.data() + p, here);
      ::write(STDOUT_FILENO, el0nl, sizeof(el0nl));
      if (end == std::string::npos)
        break;
      p += here + 1;
      for (int j = 0; j < input_start_col; ++j)
        ::write(STDOUT_FILENO, " ", 1);
    }
  }


  void insert(const char* s, size_t n)
  {
    assert(pos >= 0 && pos <= res.size());
    while (n > 0) {
      auto[x,y] = coords(pos);
      auto endp = static_cast<const char*>(memchr(s, '\n', n));
      auto here = endp == nullptr ? n : (endp - buf);
      res.insert(pos, s, here);
      ::write(STDOUT_FILENO, s, here);
      pos += here;
      if (endp == nullptr)
        break;
      res.insert(pos, 1, '\n');
      ::write(STDOUT_FILENO, el0nl, sizeof(el0nl));
      ++pos;
      if (y + 1 == cur_height)
        input_start_row -= 1;
      for (int j = 0; j < input_start_col; ++j)
        ::write(STDOUT_FILENO, " ", 1);
      s += here + 1;
      n -= here + 1;
    }
    redisplay(pos);
    auto[x,y] = move(pos);
    target_col = x - input_start_col;
  }


  void del(size_t p, size_t n)
  {
    res.erase(p, n);
    redisplay(p);
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
    ::write(STDOUT_FILENO, dsr, sizeof(dsr));
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
        while (rp < nread) {
          auto lastch = buf[wp++] = buf[rp++];
          auto pres = sm.in(lastch);
          if (pres == input_sm::parsed::parsing)
            continue;

          switch (pres) {
          case input_sm::parsed::eol:
            goto out;
          case input_sm::parsed::nl:
            // Translate for Unix systems.
            buf[wp - 1] = '\n';
            insert(reinterpret_cast<char*>(buf), wp);
            break;
          case input_sm::parsed::ch:
            insert(reinterpret_cast<char*>(buf), wp);
            break;
          case input_sm::parsed::bs:
            ::write(11,"a",1);
            if (pos > 0) {
            ::write(11,"b",1);
              del(--pos, 1);
              target_col = pos;
            }
            break;
          case input_sm::parsed::sigint:
            // XYZ Indicate user interrupt
            res = "";
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
              target_col = x - input_start_col;
            }
            break;
          case input_sm::parsed::right:
            if (pos < res.size()) {
              ++pos;
              auto[x,_] = move();
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
            }
            break;
          case input_sm::parsed::cpr:
            {
              auto nrs = numeric_parms<2>(reinterpret_cast<const char*>(buf) + 2, wp - 2, 'R');

              if (! received_position) {
                if (nrs.has_value()) {
                  // Finally show the prompt:
                  ::write(STDOUT_FILENO, prompt.c_str(), prompt.size());
                  prompt_row = (*nrs)[0] - 1;
                  prompt_col = (*nrs)[1] - 1;
                  received_position = true;
                }
                ::write(STDOUT_FILENO, dsr, sizeof(dsr));
              } else {
                if (nrs.has_value()) {
                  input_start_row = (*nrs)[0] - 1;
                  input_start_col = (*nrs)[1] - 1;
                } else
                  ::write(STDOUT_FILENO, dsr, sizeof(dsr));
              }
            }
            break;
          default:
            // Ignore for now.
            break;
          }

          wp = 0;
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
