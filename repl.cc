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
      creturn,
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
      if (ec != std::errc() || buf[newpos] != (i + 1 == N ? suffix : ';')) {
        write(1,"fail\n",5);
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


  int prompt_row = -1;
  int prompt_col = -1;

  int input_start_row = -1;
  int input_start_col = -1;

  std::string res;
  size_t pos = 0;

  bool insert_mode = true;


  std::pair<int,int> move(size_t p)
  {
    int x = input_start_col;
    int y = input_start_row;
    for (size_t i = 0; i < p; ++i)
      if (res[i] == '\n') {
        x = input_start_col;
        ++y;
      } else
        ++x;
    auto s = std::format("\e[{};{}H", 1 + y, 1 + x);
    ::write(STDOUT_FILENO, s.c_str(), s.size());
    return std::make_pair(x, y);
  }

  void move()
  {
    (void) move(pos);
  }


  void redisplay(size_t p)
  {
    auto[x,y] = move(p);
    size_t i = p;
    while (i < res.size()) {
      auto nn = res.find('\n', i);
      auto nnn = nn == std::string::npos ? res.size() - i : nn;
      ::write(STDOUT_FILENO, res.c_str() + i, nnn);
      if (nn != std::string::npos) {
        for (int j = 0; j < input_start_col; ++j)
          ::write(STDOUT_FILENO, " ", 1);
      }
      i += nnn;
    }
    move();
  }


  void insert(const char* s, size_t n)
  {
    assert(pos >= 0 && pos <= res.size());
    if (pos == res.size()) {
      res.append(s, n);
      ::write(STDOUT_FILENO, s, n);
      pos += n;
      return;
    }
    if (! insert_mode)
      res.erase(pos, n);
    res.insert(pos, s, n);
    pos += n;
    redisplay(pos - n);
  }


  std::string read(const std::string& prompt)
  {
    res.clear();
    pos = 0;

    prompt_row = -1;
    prompt_col = -1;

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
          case input_sm::parsed::ch:
            if (lastch == '\r')
              // Translate for Unix systems.
              buf[wp - 1] = '\n';

            insert(reinterpret_cast<char*>(buf), wp);
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
              move();
            }
            break;
          case input_sm::parsed::right:
            if (pos < res.size()) {
              ++pos;
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