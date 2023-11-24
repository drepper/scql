#include "scql.hh"

using namespace std::literals;


namespace scql {

  std::string location::format() const
  {
    return std::format("({}:{}-{}:{})", first_line, first_column, last_line, last_column);
  }


  std::string list::format() const
  {
    auto s = std::format("{{list{}", lloc.format());
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


  bool list::fixup(std::string&, size_t, int, int) const
  {
    return false;
  }


  std::string pipeline::format() const
  {
    auto s = std::format("{{pipeline{}", lloc.format());
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


  bool pipeline::fixup(std::string&, size_t, int, int) const
  {
    return false;
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


  std::string fcall::format() const
  {
    return std::format("{{fcall{} {} [{}]}}", lloc.format(), fname ? fname->format() : "<UNKNOWN>"s, args ? args->format() : ""s);
  }


  bool fcall::fixup(std::string& s, size_t p, int x, int y) const
  {
    if (missing_close && y == lloc.last_line && x == lloc.last_column) {
      s.insert(p, "]");
      return true;
    }
    return false;
  }


  part::cptr_type result;

} // namespace scql
