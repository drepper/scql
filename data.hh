#ifndef _DATA_HH
#define _DATA_HH 1

#include <cstdint>
#include <list>
#include <string>
#include <tuple>
#include <vector>


namespace scql::data {

  enum struct data_type {
    u8,
    u32,
    f32,
    f64,
    str,
  };

  // Scheme representation.
  struct schema {
    struct dimen {
      data_type type;
      size_t size;
      std::string label;
    };

    std::string title;
    std::vector<dimen> info;
    size_t cnt;
    void* data;
  };

  // Available data cells.
  struct data_info {
    data_info();

    std::vector<std::string> check(const std::string& pfx);

    const schema& get(const std::string& s) const;
    schema& get(const std::string& s);

    void add(const std::string& name, schema s);

  private:
    std::list<std::tuple<std::string,schema>> known;
  };

  extern data_info available;

} // namespace scql::data

  // MNIST data.
  extern uint8_t mnist_images[54880000];
  extern uint8_t mnist_labels[70000];
  // Iris flower data.
  extern uint8_t iris_data[4800];

#endif // data.hh
