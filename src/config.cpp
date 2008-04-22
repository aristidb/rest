// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/config.hpp"

#include <cassert>
#include <cstdlib>
#include <iterator>
#include <iostream>
#include <algorithm>
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

      void data(std::string const &d) {
        data_ = d;
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

  void property::name(std::string const &n) {
    impl_->name = n;
  }

  std::string const &property::data() const {
    return impl_->data.data();
  }

  void property::data(std::string const &d) {
    impl_->data.data(d);
  }

  property_tree::~property_tree() {
    for(children_iterator i = children.begin();
        i != children.end();
        ++i)
      delete *i;
  }

  void read_config(fs::path const &path, property_tree &root) {
    fs::directory_iterator end_iter;
    for(fs::directory_iterator iter(path);
        iter != end_iter;
        ++iter)
      if(fs::is_directory(*iter)) {
        property_tree::children_iterator i = root.find_children(iter->leaf());
        if(i == root.children_end()) {
          property_tree *child = new property_tree(iter->leaf());
          try {
            read_config(*iter, *child);
            root.add_child(child);
          } catch (...) {
            delete child;
            throw;
          }
        } else {
          property_tree *child = *i;
          read_config(*iter, *child);
        }
      }
      else
        root.add_property(property(iter->leaf(), *iter));
  }
}

#ifndef DEFAULT_CONFIG_PATH
    #define DEFAULT_CONFIG_PATH "./conf"
#endif

  static void usage(int /*argc*/, char **argv) {
    std::cerr << "usage: " << argv[0] << " -c <config> -h\n"
      "\n-c\tsets path to config\n";
    std::exit(3);
  }

  config::config() {
    // set defaults
    utils::set(tree_, "musikdings.rest/0.1", "general", "name");
  }

  void config::load(int argc, char **argv, char const *cpath,
                    bool allow_params)
  {
    char const * config_path = !cpath ? DEFAULT_CONFIG_PATH : cpath;
    if(allow_params)
      for(int i = 1; i < argc; ++i)
        if(argv[i][0] == '-') {
          if(argv[i][1] == 'h' && argv[i][2] == 0) {
            usage(argc, argv);
          }
          else if(argv[i][1] == 'c' && argv[i][2] == 0) {
            if(i+1 < argc) {
              config_path = argv[++i];
            }
            else {
              std::cerr << "error: no path to config given\n";
              usage(argc, argv);
            }
          }
        }

    utils::read_config(config_path, tree_);
  }

  config &config::get() {
    static config *instance = 0;
    if (!instance)
      instance = new config;
    return *instance;
  }

  utils::property_tree &config::tree() {
    return tree_;
  }
}
