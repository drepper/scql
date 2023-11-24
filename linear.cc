#include "scql.hh"


namespace scql {

  linear::linear(part::cptr_type& root)
  {
    root->prefix_map([this](part::cptr_type p) { items.emplace_back(p->lloc, p); });
  }


  std::vector<part::cptr_type> linear::at(int x, int y) const
  {
    std::vector<part::cptr_type> res;

    for (auto& e : items)
      if (((e.p->lloc.first_line == y && e.p->lloc.first_column <= x) || e.p->lloc.first_line < y)
          && ((e.p->lloc.last_line == y && e.p->lloc.last_column > x) || e.p->lloc.last_line > y))
        res.emplace_back(e.p);

    return res;
  }

} // namespace scql
