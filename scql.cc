#include "scql.hh"
#include "code.hh"

#include <algorithm>
#include <cassert>

using namespace std::literals;


namespace scql {

  std::string location::format() const
  {
    return std::format("({}:{}-{}:{})", first_line, first_column, last_line, last_column);
  }


  bool part::expandable() const
  {
    return id == id_type::ident || id == id_type::datacell || id == id_type::codecell || id == id_type::computecell;
  }


  void part::prefix_map(std::function<void(part::cptr_type)> fct)
  {
    fct(shared_from_this());
  }


  std::string list::format() const
  {
    auto s = std::format("{{{}{} ", name, lloc.format());
    bool first = true;
    for (const auto& e : l) {
      if (! first)
        s += ", ";
      if (e)
        s += e->format();
      else
        s += "<INVALID>";
      first = false;
    }
    s += "}";
    return s;
  }


  bool list::fixup(std::string& s, size_t p, int x, int y) const
  {
    for (const auto& e : l)
      if (e && e->fixup(s, p, x, y))
        return true;
    return false;
  }


  void list::prefix_map(std::function<void(part::cptr_type)> fct)
  {
    fct(shared_from_this());
    for (auto& e : l)
      if (e)
        e->prefix_map(fct);
  }


  std::string pipeline::format() const
  {
    auto s = std::format("{{pipeline{} ", lloc.format());
    bool first = true;
    for (const auto& e : l) {
      if (! first)
        s += " | ";
      if (e)
        s += e->format();
      else
        s += "<INVALID>";
      first = false;
    }
    s += "}";
    return s;
  }


  bool pipeline::fixup(std::string& s, size_t p, int x, int y) const
  {
    for (const auto& e : l)
      if (e && e->fixup(s, p, x, y))
        return true;
    return false;
  }


  void pipeline::prefix_map(std::function<void(part::cptr_type)> fct)
  {
    fct(shared_from_this());
    for (auto& e : l)
      if (e)
        e->prefix_map(fct);
  }


  std::string integer::format() const
  {
    return std::format("{{integer{}}}", lloc.format());
  }


  bool integer::fixup(std::string&, size_t, int, int) const
  {
    return false;
  }


  std::string floatnum::format() const
  {
    return std::format("{{floatnum{}}}", lloc.format());
  }


  bool floatnum::fixup(std::string&, size_t, int, int) const
  {
    return false;
  }


  std::string glob::format() const
  {
    return std::format("{{glob{}}}", lloc.format());
  }


  bool glob::fixup(std::string&, size_t, int, int) const
  {
    return false;
  }


  std::string string::format() const
  {
    return std::format("{{string{}}}", lloc.format());
  }


  bool string::fixup(std::string& s, size_t p, int x, int y) const
  {
    if (missing_close && y == lloc.last_line && x == lloc.last_column) {
      s.insert(p, "\"");
      return true;
    }
    return false;
  }


  std::string ident::format() const
  {
    return std::format("{{ident{}}}", lloc.format());
  }


  bool ident::fixup(std::string&, size_t, int, int) const
  {
    return false;
  }


  std::string datacell::format() const
  {
    return std::format("{{datacell{}}}", lloc.format());
  }


  std::string codecell::format() const
  {
    return std::format("{{codecell{}}}", lloc.format());
  }


  std::string computecell::format() const
  {
    return std::format("{{computecell{}}}", lloc.format());
  }


  std::string fcall::format() const
  {
    auto res = std::format("{{fcall{} {} [", lloc.format(), fname ? fname->format() : "<UNKNOWN>"s);

    bool first = true;
    for (const auto& e : args) {
      std::format_to(std::back_inserter(res), "{}{}", (first ? "" : ", "), e ? e->format() : "UNKNOWN"s);
      first = false;
    }
    res += "]}";
    return res;
  }


  bool fcall::fixup(std::string& s, size_t p, int x, int y) const
  {
    if (missing_close && y == lloc.last_line && x == lloc.last_column) {
      s.insert(p, "]");
      return true;
    }
    return false;
  }


  void fcall::prefix_map(std::function<void(part::cptr_type)> fct)
  {
    fct(shared_from_this());
    if (fname != nullptr)
      fct(fname);
    for (const auto& e : args)
      if (e != nullptr)
        e->prefix_map(fct);
  }


  part::cptr_type result;


  void annotate(part::cptr_type& p, std::vector<data::schema*>* last)
  {
    if (p->id != id_type::pipeline)
      return;

    auto pl = as<pipeline>(p);

    std::vector<data::schema*> cur;
    if (last != nullptr)
      cur = *last;

    for (auto& e : pl->l) {
      if (e == nullptr) {
        cur.clear();
        continue;
      }

      std::vector<data::schema*> next;

      e->errmsg.clear();
      assert(e->is(id_type::statements));
      auto stmts = as<statements>(e);
      for (auto& ee : stmts->l) {
        if (ee == nullptr) {
          next.push_back(nullptr);
          continue;
        }
        ee->errmsg.clear();
        if (ee->is(id_type::pipeline))
          annotate(ee, last);
        else if (ee->is(id_type::datacell)) {
          auto d = scql::as<scql::datacell>(ee);
          if (auto av = scql::data::available.match(d->val); av.size() == 1 && av[0] == d->val) {
            d->shape = scql::data::available.get(d->val);
            next.push_back(&d->shape);
          } else
            next.push_back(nullptr);
        } else if (ee->is(id_type::fcall)) {
          auto f = scql::as<scql::fcall>(ee);
          if (f->fname && f->fname->is(id_type::ident)) {
            auto fname = as<scql::ident>(f->fname)->val;
            if (auto av = scql::code::available.match(fname); av.size() == 1 && av[0] == fname) {
              auto& fct = scql::code::available.get(fname);

              f->known = true;

              auto oshape = fct.output_shape(cur.empty() ? nullptr : cur.size() == 1 ? cur[0] : cur[next.size()], f->args);
              if (std::holds_alternative<std::string>(oshape)) {
                if (auto& s = std::get<std::string>(oshape); ! s.empty()) {
                  f->errmsg = s;
                }
              } else {
                f->shape = std::get<data::schema>(oshape);
              }
            }
          }
        }
      }
      cur = std::move(next);
    }
  }


  bool valid(part::cptr_type& p)
  {
    switch (p->id) {
    case id_type::datacell:
      return p->shape;
    case id_type::fcall:
      return as<fcall>(p)->fname->is(id_type::ident) && as<fcall>(p)->known && p->shape;
    case id_type::pipeline:
      if (as<pipeline>(p)->l.empty())
        return false;
      for (auto& e : as<pipeline>(p)->l)
        if (e == nullptr || ! valid(e))
          return false;
      return true;
    case id_type::statements:
      if (as<statements>(p)->l.empty())
        return false;
      for (auto& e : as<statements>(p)->l)
        if (e == nullptr || ! valid(e))
          return false;
      return true;
    case id_type::integer:
    case id_type::floatnum:
    case id_type::string:
      return true;
    default:
      break;
    }
    return false;
  }


} // namespace scql
