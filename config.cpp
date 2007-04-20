#include <string>
#include <algorithm>
#include <boost/ref.hpp>
#include <boost/variant.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

using namespace boost::multi_index;
namespace fs = boost::filesystem;

namespace rest {
namespace utils {
  namespace {
    class property {
      std::string const name_;

      class data_handler {
      public:
        data_handler(fs::path const &file)
          : data_(file)
        { }
        data_handler(std::string const &data)
          : data_(data)
        { }

        std::string const &data() const {
          std::string const *data;
          if( (data = boost::get<std::string>(&data_)) )
            return *data;
          else {
            read_data();
            return boost::get<std::string>(data_);
          }
        }
      private:
        void read_data() const {
          fs::path const &file = boost::get<fs::path>(data_);
          std::string data;

          fs::ifstream in(file);
          std::copy(std::istream_iterator<char>(in),
                    std::istream_iterator<char>(),
                    std::back_inserter(data));

          data_ = boost::variant<fs::path, std::string>(data);
        }

        mutable boost::variant<fs::path, std::string> data_;
      } const data_;
    public:
      property(std::string const &name,
               fs::path const &file)
        : name_(name), data_(file)
      { }

      property(std::string const &name,
               std::string const &data)
        : name_(name), data_(data)
      { }

      std::string const &name() const {
        return name_;
      }
      std::string const &data() const {
        return data_.data();
      }
    };

    // TODO aus Effizienz Gründen lieber ein eigener Extractor um Kopien zu verhindern
#define CONST_REF_MEM_FUN(Class, Type, MemberFunName) \
    ::boost::multi_index::const_mem_fun_explicit< \
      Class, Type, Type const &(Class::*)()const, &Class::MemberFunName>

    class property_tree {
    public:
      std::string const &name() const {
        return name_;
      }
    private:
      typedef boost::multi_index_container<
        property_tree*,
        indexed_by<
          hashed_unique<
            CONST_REF_MEM_FUN(property_tree, std::string, name)
            >
          >
         > children_t;

      typedef boost::multi_index_container<
        property,
        indexed_by<
          hashed_unique<
            CONST_REF_MEM_FUN(property, std::string, name)
            >
          >
         > property_t;

      children_t children;
      property_t properties;
      std::string name_;
    public:
      typedef property_t::const_iterator property_iterator;
      property_iterator property_begin() const {
        return properties.begin();
      }
      property_iterator property_end() const {
        return properties.end();
      }
      property_iterator find_property(std::string const &name) const {
        return properties.find(name);
      }

      typedef children_t::const_iterator children_iterator;
      children_iterator children_begin() const {
        return children.begin();
      }
      children_iterator children_end() const {
        return children.end();
      }
      children_iterator find_children(std::string const &name) const {
        return children.find(name);
      }
    };
  }
}}
