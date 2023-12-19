#include "code.hh"

#include <algorithm>
#include <cerrno>
#include <format>
#include <utility>

using namespace std::literals;


namespace scql::code {

  namespace {

    std::variant<std::vector<data::schema>,std::string> reshape_output_shape(const std::vector<data::schema*>& in_schema, const std::vector<part::cptr_type>& args)
    {
      // The parameters are supposed to be positive integers or the glob.
      std::vector<intmax_t> req;

      if (args.empty())
        return std::format("dimensions required");

      bool has_glob = false;
      intmax_t old_multiple = 1;
      intmax_t multiple = 1;
      for (auto e : args)
        if (e == nullptr) {
          return "empty parameter not allowed";
        } else
          switch (e->id) {
          case id_type::integer:
            req.push_back(as<integer>(e)->val);
            if (req.back() <= 0)
              goto invalid;
            if (__builtin_mul_overflow(multiple, req.back(), &multiple))
              return std::format("requested dimensions too high");
            break;
          case id_type::glob:
            req.push_back(0);
            has_glob = true;
            break;
          default:
          invalid:
            return std::format("invalid argument {}\nmust be a positive integer or glob", e->format());
          }

      if (in_schema.empty())
        return "reshapes requires input data";

      std::vector<data::schema> res;
      for (auto& is : in_schema) {
        for (auto m : is->dimens)
          old_multiple *= m;

        if (old_multiple < multiple)
          return std::format("requested dimensions too high");
        if (old_multiple % multiple != 0)
          return std::format("defined sizes have remainder of {}", old_multiple % multiple);

        res.push_back(data::schema { "", is->columns, { }, is->data });
        for (auto m : req)
          if (m == 0) {
            res.back().dimens.push_back(has_glob ? old_multiple / multiple : 1);
            has_glob = false;
          } else
            res.back().dimens.push_back(m);
      }

      return res;
    }

    std::vector<data::schema> reshape(const std::vector<data::schema*>& in_schema, const std::vector<part::cptr_type>& args)
    {
      return std::get<std::vector<data::schema>>(reshape_output_shape(in_schema, args));
    }

    function reshape_info {
      reshape_output_shape,
      reshape
    };


    std::variant<std::vector<data::schema>,std::string> zip_output_shape(const std::vector<data::schema*>& in_schema, const std::vector<part::cptr_type>& args)
    {
      if (! args.empty())
        return "zip does not expect arguments"s;

      size_t idx = 0;
      while (idx < in_schema[0]->dimens.size()) {
        if (std::ranges::any_of(in_schema, [idx, v = in_schema[0]->dimens[idx]](const auto& e) { return idx >= e->dimens.size() || e->dimens[idx] != v; }))
          break;
        ++idx;
      }

      if (idx == 0)
        return "no common dimensionality";

      data::schema res;

      for (size_t i = 0; i < idx; ++i)
        res.dimens.push_back(in_schema[0]->dimens[i]);

      for (const auto s : in_schema)
        for (const auto& c : s->columns) {
          auto& n = res.columns.emplace_back(c);
          for (size_t i = idx; i < s->dimens.size(); ++i)
            n.dimens.insert(n.dimens.begin() + (i - idx), s->dimens[i]);
          while (n.dimens.size() > 1 && n.dimens.back() == 1)
            n.dimens.pop_back();
        }

      return std::vector { res };
    }

    std::vector<data::schema> zip(const std::vector<data::schema*>& in_schema, const std::vector<part::cptr_type>& args)
    {
      (void) args;
      std::vector<data::schema> res;
      for (const auto p : in_schema)
        res.emplace_back(*p);
      return res;
    }

    function zip_info {
      zip_output_shape,
      zip
    };


  } // anonymous namespace


  code_info::code_info()
  : known { }
  {
    known.emplace_back(std::make_tuple("reshape"s, &reshape_info));
    known.emplace_back(std::make_tuple("zip"s, &zip_info));
  }


  const function& code_info::get(const std::string& s) const
  {
    for (const auto& e : known)
      if (std::get<std::string>(e) == s)
        return *std::get<function*>(e);
    std::unreachable();
  }


  function& code_info::get(const std::string& s)
  {
    for (auto& e : known)
      if (std::get<std::string>(e) == s)
        return *std::get<function*>(e);
    std::unreachable();
  }



  std::vector<std::string> code_info::match(const std::string& pfx)
  {
    std::vector<std::string> res;
    for (auto[n,_] : known)
      if (n.starts_with(pfx))
        res.emplace_back(n);
    return res;
  }

  code_info available;

} // namespace scql::data
