#include <cstdlib>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

using namespace std::literals;



struct parts_type {
  enum struct id_type {
    eot,
    single,
    error,
    number,
    variable,
    string,
    function,
    reference,
  };

  parts_type(id_type id_, bool complete_) : id(id_), complete(complete_) { }

  virtual std::string format(const std::string_view& s) const = 0;

  auto complete_p() const { return complete; }

  id_type id;
  bool complete;
};


struct eot_type : parts_type {
  eot_type(bool complete_) : parts_type(parts_type::id_type::eot, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<error_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return s; }
};


struct single_type : parts_type {
  single_type(bool complete_) : parts_type(parts_type::id_type::single, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<error_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return s; }
};


struct error_type : parts_type {
  error_type(bool complete_) : parts_type(parts_type::id_type::error, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<error_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return "\e[30;41m"s + s + "\e[0m"; }
};


struct number_type : parts_type {
  number_type(bool complete_) : parts_type(parts_type::id_type::number, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<number_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return "\e[33m"s + s + "\e[0m"; }
};


struct variable_type : parts_type {
  variable_type(bool complete_) : parts_type(parts_type::id_type::variable, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<variable_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return "\e[32m"s + s + "\e[0m"; }
};


struct string_type : parts_type {
  string_type(bool complete_) : parts_type(parts_type::id_type::string, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<string_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return "\e[35m"s + s + "\e[0m"; }
};


struct function_type : parts_type {
  function_type(bool complete_) : parts_type(parts_type::id_type::function, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<function_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return "\e[36m"s + s + "\e[0m"; }
};


struct reference_type : parts_type {
  reference_type(bool complete_) : parts_type(parts_type::id_type::reference, complete_) { }

  static std::unique_ptr<parts_type> alloc(const std::string_view& sv, bool complete_ = false) { return std::make_unique<function_type>(complete_); }
  virtual std::string format(const std::string_view& s) const override { return "\e[32m"s + s + "\e[0m"; }
};


std::map<parts_type::id_type,std::unique_ptr<parts_type>(*)(const std::string_view&,bool)> id_map {
  { parts_type::id_type::eot, eot_type::alloc },
  { parts_type::id_type::number, number_type::alloc },
  { parts_type::id_type::variable, variable_type::alloc },
  { parts_type::id_type::string, string_type::alloc },
  { parts_type::id_type::function, function_type::alloc },
  { parts_type::id_type::reference, reference_type::alloc },
};


struct token_type {
  token_type(const std::string_view& s, size_t from_, size_t to_, parts_type::id_type id, bool complete_)
  : from(from_), to(to_), part(id_map[id](std::string_view(s.data() + from_, to_), complete_)) { }
  token_type(const token_type&) = delete;
  token_type& operator=(const token_type&) = delete;

  static std::unique_ptr<token_type> alloc(const std::string_view& s, size_t from_, size_t to_, parts_type::id_type i, bool complete_) { return std::make_unique<token_type>(s, from_, to_, i, complete_); }

  size_t from;
  size_t to;

  std::unique_ptr<parts_type> part;
};


std::pair<std::unique_ptr<token_type>,size_t> scan(const std::string_view& s, size_t off)
{
  while (off < s.size() && ! isspace(s[off]))
    ++off;

  if (off == s.size())
    return std::make_pair(token_type::alloc(s, off, off, parts_type::id_type::eot, false), off);

  if (s[off] == '$') {
    if (off + 1 == s.size())
      return std::make_pair(token_type::alloc(s, off, off + 1, parts_type::id_type::reference, false), off + 1);
    if (s[off + 1] == '[') {
      auto last = s.find_first_of(']', off + 2);
      if (last == std::string_view::npos)
        return std::make_pair(token_type::alloc(s, off, s.size(), parts_type::id_type::reference, false), last);

      return std::make_pair(token_type::alloc(s, off, last + 1, parts_type::id_type::reference, true), last + 1);
    } else {
      auto last = s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ:"sv, off + 1);
      if (last == std::string_view::npos)
        last = s.size();
      return std::make_pair(token_type::alloc(s, off, last, parts_type::id_type::reference, last > 0), last);
    }
  }

  if (isdigit(s[off])) {
    auto last = s.find_first_not_of("0123456789"sv, off);
    if (last != std::string_view::npos && s[last] == '.')
      last = s.find_first_not_of("0123456789"sv, off);
    if (last == std::string_view::npos)
      last = s.size();
    return std::make_pair(token_type::alloc(s, off, last, parts_type::id_type::number, true), last);
  }

  if (s[off] == '"' || s[off] == '\'') {
    char q = s[off];
    bool triple = false;

    auto last = off;
    if (off + 2 < s.size() && s[off + 1] == q && s[off + 2] == q) {
      triple = true;
      last += 3;
    } else
      last += 1;

    bool complete = false;
    while (last < s.size()) {
      if (s[last] == q) {
        if (! triple) {
          last += 1;
          break;
        }
        if (last + 2 < s.size() && s[last + 1] == q && s[last + 2] == q) {
          last += 3;
          break;
        }
      }
      ++last;
    }

    return std::make_pair(token_type::alloc(s, off, last, parts_type::id_type::string, true), last);
  }

  return std::make_pair(token_type::alloc(s, off, off + 1, parts_type::id_type::single, false), off + 1);
}


struct tree_type {
  using atom_type = std::unique_ptr<token_type>;
  using seq_type = std::list<std::unique_ptr<tree_type>>;

  enum struct node_type {
    atom,
    fcall,
    seq,
  };

  tree_type(node_type t_) : t(t_) { }

  static std::unique_ptr<tree_type> alloc_atom(atom_type& v) { auto res = std::make_unique<tree_type>(node_type::atom); res->val = std::move(v); return res; }
  static std::unique_ptr<tree_type> alloc_fcall(atom_type& v) { auto res = std::make_unique<tree_type>(node_type::fcall); res->val = std::move(v); return res; }
  static std::unique_ptr<tree_type> alloc_seq() { return std::make_unique<tree_type>(node_type::seq); }

  void add(std::unique_ptr<tree_type>& n) { std::get<seq_type>(val).emplace_back(std::move(n)); }

  node_type t;
  std::variant<atom_type,seq_type> val;
};


enum struct state_type {
  top,
};


std::pair<std::unique_ptr<tree_type>,size_t> parse(const std::string_view& s, size_t off)
{
  state_type state = state_type::top;
  auto res = tree_type::alloc_seq();

  while (true) {
    auto[tok,newoff] = scan(s, off);
    if (tok->part->id == parts_type::id_type::eot)
      break;
  }

  return std::make_pair(std::move(res), off);
}
