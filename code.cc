#include "code.hh"

#include <format>
#include <utility>

using namespace std::literals;


namespace scql::code {

  namespace {

    std::variant<data::schema,std::string> reshape_output_shape(const data::schema* in_schema, const std::vector<part::cptr_type>& args)
    {
      // The parameters are supposed to be positive integers or the glob.
      std::vector<intmax_t> req;

      if (args.empty())
        return std::format("dimensions required");

      bool has_glob = false;
      intmax_t old_multiple = 1;
      intmax_t multiple = 1;
      for (auto e : args)
        if (e)
          switch (e->id) {
          case id_type::integer:
            req.push_back(as<integer>(e)->val);
            if (req.back() <= 0)
              goto invalid;
            if (__builtin_mul_overflow(multiple, req.back(), &multiple))
              goto toohigh;
            break;
          case id_type::glob:
            req.push_back(0);
            has_glob = true;
            break;
          default:
          invalid:
            return std::format("invalid argument {}\nmust be a positive integer or glob", e->format());
          }

      if (in_schema == nullptr)
        return "reshapes requires input data";

      for (auto m : in_schema->dimens)
        old_multiple *= m;

      if (old_multiple < multiple)
      toohigh:
        return std::format("requested dimensions too high");
      if (old_multiple % multiple != 0)
        return std::format("defined sizes have remainder of {}", old_multiple % multiple);

      data::schema res { "", in_schema->columns, { }, in_schema->data };
      for (auto m : req)
        if (m == 0) {
          res.dimens.push_back(has_glob ? old_multiple / multiple : 1);
          has_glob = false;
        } else
          res.dimens.push_back(m);

      return res;
    }

    data::schema reshape(const data::schema* in_schema, const std::vector<part::cptr_type>& args)
    {
      return std::get<data::schema>(reshape_output_shape(in_schema, args));
    }

    function reshape_info {
      reshape_output_shape,
      reshape
    };

  } // anonymous namespace


  code_info::code_info()
  : known { }
  {
    known.emplace_back(std::make_tuple("reshape"s, &reshape_info));
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
