#include <utility>

#include "data.hh"

using namespace std::literals;


namespace scql::data {

  data_info::data_info()
  : known { }
  {
    known.emplace_back(std::make_tuple("mnist_images"s, schema { "MNIST image data"s, { schema::dimen { data_type::u8, 54880000zu, ""s } }, 1, static_cast<void*>(mnist_images) }));
    known.emplace_back(std::make_tuple("mnist_labels"s, schema { "MNIST image label"s, { schema::dimen { data_type::u8, 70000zu, ""s } }, 1, static_cast<void*>(mnist_labels) }));

    known.emplace_back(std::make_tuple("iris_data"s, schema { "Fisher's Iris data set"s, { schema::dimen { data_type::str, 4zu, ""s }, schema::dimen { data_type::f32, 1zu, "Sepal.Width"s }, schema::dimen { data_type::f32, 1zu, "Sepal.Width"s }, schema::dimen { data_type::f32, 1zu, "Petal.Length"s }, schema::dimen { data_type::f32, 1zu, "Petal.Width"s }, schema::dimen { data_type::str, 12zu, "Species"s }, }, 150, static_cast<void*>(iris_data) }));
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



  std::vector<std::string> data_info::check(const std::string& pfx)
  {
    std::vector<std::string> res;
    for (auto[n,_] : known)
      if (n.starts_with(pfx))
        res.emplace_back(n);
    return res;
  }



  data_info available;

} // namespace scql::data
