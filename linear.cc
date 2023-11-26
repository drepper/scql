#include "scql.hh"


namespace scql {

  linear::linear(part::cptr_type& root)
  {
    root->prefix_map([this](part::cptr_type p) { items.emplace_back(p->lloc, p); });
  }


  std::vector<linear::item*> linear::at(int x, int y)
  {
    std::vector<item*> res;

    for (size_t i = 0; i < items.size(); ++i)
      if (((items[i].p->lloc.first_line == y && items[i].p->lloc.first_column <= x) || items[i].p->lloc.first_line < y)
          && ((items[i].p->lloc.last_line == y && items[i].p->lloc.last_column > x) || items[i].p->lloc.last_line > y))
        res.emplace_back(&items[i]);

    return res;
  }

} // namespace scql
