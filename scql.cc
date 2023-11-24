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
    for (const auto e : l) {
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


  std::string pipeline::format() const
  {
    auto s = std::format("{{pipeline{}", lloc.format());
    bool first = true;
    for (const auto e : l) {
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


  std::string integer::format() const
  {
    return std::format("{{integer{}}}", lloc.format());
  }


  std::string floatnum::format() const
  {
    return std::format("{{floatnum{}}}", lloc.format());
  }


  std::string string::format() const
  {
    return std::format("{{string{}}}", lloc.format());
  }


  std::string ident::format() const
  {
    return std::format("{{ident{}}}", lloc.format());
  }


  std::string fcall::format() const
  {
    return std::format("{{fcall{} {} [{}]}}", lloc.format(), fname ? fname->format() : "<UNKNOWN>"s, args ? args->format() : ""s);
  }


  part::cptr_type result;

} // namespace scql
