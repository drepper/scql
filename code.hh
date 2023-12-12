#ifndef _CODE_HH
#define _CODE_HH 1

#include "scql.hh"
#include "data.hh"

#include <variant>


namespace scql::code {

  struct function {
    using t_output_shape = std::variant<data::schema,std::string> (*)(const data::schema*, const std::vector<part::cptr_type>&);
    using t_operate = data::schema (*)(const data::schema*, const std::vector<part::cptr_type>&);

    function(t_output_shape f_output_shape_, t_operate f_operate_)
    : f_output_shape(f_output_shape_), f_operate(f_operate_)
    { }
    function(const function&) = delete;
    function operator=(const function&) = delete;

    std::variant<data::schema,std::string> output_shape(const data::schema* in_schema, const std::vector<part::cptr_type>& args) const { return f_output_shape(in_schema, args); }

    data::schema operator()(const data::schema* in_schema, const std::vector<part::cptr_type>& args) const { return f_operate(in_schema, args); }

  private:
    t_output_shape f_output_shape;
    t_operate f_operate;
  };


  struct code_info {
    code_info();

    std::vector<std::string> match(const std::string& pfx);

    const function& get(const std::string& s) const;
    function& get(const std::string& s);

    void add(const std::string& name, function s);

  private:
    std::list<std::tuple<std::string,function*>> known;
  };


  extern code_info available;

} // namespace scql::code

#endif // code.hh
