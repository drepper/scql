#ifndef _SCQL_HH
#define _SCQL_HH 1

#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// XYZ Debugging
// #include <iostream>


namespace scql {

  enum struct id_type {
    integer,
    floatnum,
    string,
    list,
    pipeline,
    datacell,
    codecell,
    computecell,
    ident,
    fcall,
  };


  struct location {
    int first_line;
    int first_column;
    int last_line;
    int last_column;

    std::string format() const;
  };
#define YYLTYPE scql::location


#pragma GCC diagnostic push
// This warning must be disable due to the std::enable_shared_from_this class.
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
  struct part : std::enable_shared_from_this<part> {
    // This type should actually be std::unique_ptr but this runs into problems with the parser generated code.
    using cptr_type = std::shared_ptr<part>;

    part(id_type id_, const location& lloc_) : id(id_), lloc(lloc_) { }
    virtual ~part() = default;

    virtual std::string format() const = 0;

    virtual bool fixup(std::string& s, size_t p, int x, int y) const = 0;

    virtual void prefix_map(std::function<void(part::cptr_type)> fct);

    bool is(id_type i) const { return id == i; }
    bool expandable() const;

    id_type id;
    location lloc;
  };
#pragma GCC diagnostic pop


  struct list : part {
    using cptr_type = std::shared_ptr<list>;

    list(const location& lloc_) : part(id_type::list, lloc_), l { } { }
    list(part::cptr_type&& p, const location& lloc_) : part(id_type::list, lloc_), l() { l.emplace_back(std::move(p)); }
    list(part::cptr_type&& p1, part::cptr_type&& p2, const location& lloc_) : part(id_type::list, lloc_), l() { l.emplace_back(std::move(p1)); l.emplace_back(std::move(p2)); }
    ~list() override = default;

    void prepend(part::cptr_type&& p) { l.emplace(l.begin(), std::move(p)); }
    void add(part::cptr_type&& p) { l.emplace_back(std::move(p)); }

    static auto alloc(const location& lloc_) { return std::make_unique<list>(lloc_); }
    static auto alloc(part::cptr_type&& p, const location& lloc_) { return std::make_unique<list>(std::move(p), lloc_); }
    static auto alloc(part::cptr_type&& p1, part::cptr_type&& p2, const location& lloc_) { return std::make_unique<list>(std::move(p1), std::move(p2), lloc_); }

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    void prefix_map(std::function<void(part::cptr_type)> fct) override;

    std::vector<part::cptr_type> l;
  };


  struct pipeline : part {
    using cptr_type = std::shared_ptr<pipeline>;

    pipeline(part::cptr_type&& p1, part::cptr_type&& p2, const location& lloc_) : part(id_type::pipeline, lloc_), l() { l.emplace_back(std::move(p1)); l.emplace_back(std::move(p2)); }
    ~pipeline() override = default;

    void prepend(part::cptr_type&& p) { l.emplace(l.begin(), std::move(p)); }

    static auto alloc(part::cptr_type&& p1, part::cptr_type&& p2, const location& lloc_) { return std::make_unique<pipeline>(std::move(p1), std::move(p2), lloc_); }

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    void prefix_map(std::function<void(part::cptr_type)> fct) override;

    std::vector<part::cptr_type> l;
  };


  struct integer : part {
    using cptr_type = std::shared_ptr<integer>;

    integer(intmax_t v, const location& lloc_) : part(id_type::integer, lloc_), val(v) { }
    ~integer() override = default;

    static auto alloc(intmax_t v, const location& lloc_) { return std::make_unique<integer>(v, lloc_); }

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    intmax_t val;
  };


  struct floatnum : part {
    using cptr_type = std::shared_ptr<floatnum>;

    using float_type = double;

    floatnum(float_type v, const location& lloc_) : part(id_type::floatnum, lloc_), val(v) { }
    ~floatnum() override = default;

    static auto alloc(float_type v, const location& lloc_) { return std::make_unique<floatnum>(v, lloc_); }

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    float_type val;
  };


  struct string : part {
    using cptr_type = std::shared_ptr<string>;

    string(const std::string& v, const location& lloc_) : part(id_type::string, lloc_), val(v) { }
    string(std::string&& v, const location& lloc_) : part(id_type::string, lloc_), val(std::move(v)) { }
    ~string() override = default;

    static auto alloc(const std::string& v, const location& lloc_) { return std::make_unique<string>(v, lloc_); }
    static auto alloc(std::string&& v, const location& lloc_) { return std::make_unique<string>(std::move(v), lloc_); }
    static auto alloc(const char*s, size_t l, const location& lloc_) { return std::make_unique<string>(std::string(s, l), lloc_); }

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    std::string val;

    bool missing_close = false;
  };


  struct ident : part {
    using cptr_type = std::shared_ptr<ident>;

    ident(const std::string& v, const location& lloc_) : part(id_type::ident, lloc_), val(v) { }
    ident(std::string&& v, const location& lloc_) : part(id_type::ident, lloc_), val(std::move(v)) { }
    ~ident() override = default;

    static auto alloc(const std::string& v, const location& lloc_) { return std::make_unique<ident>(v, lloc_); }
    static auto alloc(std::string&& v, const location& lloc_) { return std::make_unique<ident>(std::move(v), lloc_); }
    static auto alloc(const char*s, size_t l, const location& lloc_) { return std::make_unique<ident>(std::string(s, l), lloc_); }

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    std::string val;

  protected:
    ident(id_type id_, const std::string& v, const location& lloc_) : part(id_, lloc_), val(v) { }
  };


  struct datacell final : public ident {
    datacell(const std::string& v, const location& lloc_) : ident(id_type::datacell, v, lloc_) { }
    datacell(std::string&& v, const location& lloc_) : ident(id_type::datacell, std::move(v), lloc_) { }
    ~datacell() override = default;

    static auto alloc(const std::string& v, const location& lloc_) { return std::make_unique<datacell>(v, lloc_); }
    static auto alloc(std::string&& v, const location& lloc_) { return std::make_unique<datacell>(std::move(v), lloc_); }
    static auto alloc(const char*s, size_t l, const location& lloc_) { return std::make_unique<datacell>(std::string(s, l), lloc_); }

    std::string format() const override;
  };


  struct codecell final : ident {
    codecell(const std::string& v, const location& lloc_) : ident(id_type::codecell, v, lloc_) { }
    codecell(std::string&& v, const location& lloc_) : ident(id_type::codecell, std::move(v), lloc_) { }
    ~codecell() override = default;

    static auto alloc(const std::string& v, const location& lloc_) { return std::make_unique<codecell>(v, lloc_); }
    static auto alloc(std::string&& v, const location& lloc_) { return std::make_unique<codecell>(std::move(v), lloc_); }
    static auto alloc(const char*s, size_t l, const location& lloc_) { return std::make_unique<codecell>(std::string(s, l), lloc_); }

    std::string format() const override;

    bool missing_brackets = false;
  };


  struct computecell final : ident {
    computecell(const std::string& v, const location& lloc_) : ident(id_type::computecell, v, lloc_) { }
    computecell(std::string&& v, const location& lloc_) : ident(id_type::computecell, std::move(v), lloc_) { }
    ~computecell() override = default;

    static auto alloc(const std::string& v, const location& lloc_) { return std::make_unique<computecell>(v, lloc_); }
    static auto alloc(std::string&& v, const location& lloc_) { return std::make_unique<computecell>(std::move(v), lloc_); }
    static auto alloc(const char*s, size_t l, const location& lloc_) { return std::make_unique<computecell>(std::string(s, l), lloc_); }

    std::string format() const override;
  };


  struct fcall : part {
    using cptr_type = std::shared_ptr<fcall>;

    fcall(part::cptr_type&& fname_, part::cptr_type&& args_, const location& lloc_) : part(id_type::fcall, lloc_), fname(std::move(fname_)), args(std::move(args_)) { }
    ~fcall() override = default;

    static auto alloc(part::cptr_type&& fname_, part::cptr_type&& args_, const location& lloc_) { return std::make_unique<fcall>(std::move(fname_), std::move(args_), lloc_); }

    part::cptr_type fname;
    part::cptr_type args;

    std::string format() const override;

    bool fixup(std::string& s, size_t p, int x, int y) const override;

    void prefix_map(std::function<void(part::cptr_type)> fct) override;

    bool missing_close = false;
  };


  template<typename T>
  inline auto as(part::cptr_type& p)
  {
    return std::static_pointer_cast<T>(p);
  }


  using yyscan_t = void*;


  struct linear {
    struct item {
      location lloc;
      part::cptr_type p;
    };

    linear() { }
    linear(part::cptr_type& root);

    auto empty() const { return items.empty(); }
    const auto& back() const { return items.back(); }
    auto& back() { return items.back(); }

    std::vector<item*> at(int x, int y);

    std::vector<item> items { };
  };




  extern part::cptr_type result;

} // namespace scql

#endif // scql.hh