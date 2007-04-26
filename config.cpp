// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-config.hpp"

#include <cassert>
#include <algorithm>
#include <iterator>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

using namespace boost::multi_index;
namespace fs = boost::filesystem;

namespace rest {
namespace utils {
  namespace {
    class data_handler {
    public:
      explicit data_handler(fs::path const &file) {
        fs::fstream in(file);
        data_.assign(
            std::istreambuf_iterator<char>(in.rdbuf()),
            std::istreambuf_iterator<char>());
      }

      explicit data_handler(std::string const &data)
        : data_(data)
      {}

      std::string const &data() const {
        return data_;
      }

    private:
      std::string data_;
    };
  }

  class property::impl {
  public:
    std::string name;
    data_handler data;

    impl(std::string const &name, fs::path const &file)
      : name(name), data(file)
    { }

    impl(std::string const &name, std::string const &data)
      : name(name), data(data)
    { }
  };

  property::property(std::string const &name, fs::path const &file)
    : impl_(new impl(name, file))
  { }

  property::property(std::string const &name,
                     std::string const &data)
    : impl_(new impl(name, data))
  { }

  std::string const &property::name() const {
    return impl_->name;
  }

  std::string const &property::data() const {
    return impl_->data.data();
  }

  class property_tree::impl_ {
  public:
    children_t children;
    property_t properties;
    std::string const name;

    impl_(std::string const &name)
      : name(name)
    { }
    impl_() { }
  };

  property_tree::property_tree()
    : impl(new impl_)
  { }

  property_tree::property_tree(std::string const &name)
    : impl(new impl_(name))
  { }

  std::string const &property_tree::name() const {
    return impl->name;
  }

  void property_tree::add_child(property_tree *child) {
    assert(child);
    impl->children.insert(child);
  }

  void property_tree::add_property(property const &p) {
    impl->properties.insert(p);
  }

  property_tree::property_iterator property_tree::property_begin() const {
    return impl->properties.begin();
  }
  property_tree::property_iterator property_tree::property_end() const {
    return impl->properties.end();
  }
  property_tree::property_iterator
  property_tree::find_property(std::string const &name) const {
    return impl->properties.find(name);
  }

  property_tree::children_iterator property_tree::children_begin() const {
    return impl->children.begin();
  }
  property_tree::children_iterator property_tree::children_end() const {
    return impl->children.end();
  }
  property_tree::children_iterator
  property_tree::find_children(std::string const &name) const {
    return impl->children.find(name);
  }

  property_tree::~property_tree() {
    for(children_iterator i = impl->children.begin();
        i != impl->children.end();
        ++i)
      delete *i;
  }

  void read_config(fs::path const &path, property_tree &root) {
    fs::directory_iterator end_iter;
    for(fs::directory_iterator iter(path);
        iter != end_iter;
        ++iter)
      if(fs::is_directory(*iter)) {
        property_tree *child = new property_tree;
        read_config(*iter, *child);
        root.add_child(child);
      } else {
        root.add_property(property(iter->leaf(), *iter));
      }
  }
}}
