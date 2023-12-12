#include <format>
#include <iterator>
#include <map>
#include <utility>

#include "data.hh"

using namespace std::literals;


namespace scql::data {

  namespace {

    std::map<data_type, std::string> type_names {
      { data_type::u8, "u8"s },
      { data_type::u32, "u32"s },
      { data_type::f32, "f32"s },
      { data_type::f64, "f64"s },
      { data_type::str, "str"s },
    };

  } // anonymous namespace

  schema::operator std::string() const
  {
    std::string res = title;

    if (! res.empty())
      res += '\n';

    for (auto n : dimens)
      std::format_to(std::back_inserter(res), "{} × ", n);

    for (const auto& c : columns)
      if (c.label.empty()) {
        // if (c.size == 1)
        //   std::format_to(std::back_inserter(res), "{} ", type_names[c.type]);
        // else
          std::format_to(std::back_inserter(res), "({} {}) ", c.size, type_names[c.type]);
      } else {
        // if (c.size == 1)
          // std::format_to(std::back_inserter(res), "({} {}) ", c.label, type_names[c.type]);
        // else
          std::format_to(std::back_inserter(res), "({} {} {}) ", c.label, c.size, type_names[c.type]);
      }

    return res;
  }


  data_info::data_info()
  : known { }
  {
    known.emplace_back(std::make_tuple("mnist_images"s, schema { "MNIST image data"s, { schema::column { data_type::u8, 1zu, ""s } }, { 54880000zu }, static_cast<void*>(mnist_images) }));
    known.emplace_back(std::make_tuple("mnist_labels"s, schema { "MNIST image label"s, { schema::column { data_type::u8, 1zu, ""s } }, { 70000zu }, static_cast<void*>(mnist_labels) }));

    known.emplace_back(std::make_tuple("iris_data"s, schema { "Fisher's Iris data set"s, { schema::column { data_type::str, 4zu, ""s }, schema::column { data_type::f32, 1zu, "Sepal.Width"s }, schema::column { data_type::f32, 1zu, "Sepal.Width"s }, schema::column { data_type::f32, 1zu, "Petal.Length"s }, schema::column { data_type::f32, 1zu, "Petal.Width"s }, schema::column { data_type::str, 12zu, "Species"s }, }, { 150zu }, static_cast<void*>(iris_data) }));
  }


  void data_info::add(const std::string& name, schema s)
  {
    known.emplace_back(std::make_tuple(name, s));
  }

  const schema& data_info::get(const std::string& s) const
  {
    for (const auto& e : known)
      if (std::get<std::string>(e) == s)
        return std::get<schema>(e);
    std::unreachable();
  }


  schema& data_info::get(const std::string& s)
  {
    for (auto& e : known)
      if (std::get<std::string>(e) == s)
        return std::get<schema>(e);
    std::unreachable();
  }



  std::vector<std::string> data_info::match(const std::string& pfx)
  {
    std::vector<std::string> res;
    for (auto[n,_] : known)
      if (n.starts_with(pfx))
        res.emplace_back(n);
    return res;
  }



  data_info available;

} // namespace scql::data
