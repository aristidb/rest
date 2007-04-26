// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <string>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <boost/ref.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

#ifdef STANDALONE
#include <iostream>
#include <iomanip>
#endif

using namespace boost::multi_index;
namespace fs = boost::filesystem;

namespace rest {
namespace utils {
  class property {
    std::string const name_;
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

  namespace {
    template<class Class,typename Type,
             Type const &(Class::*PtrToMemberFunction)() const>
    struct ref_const_mem_fun_const {
      typedef typename boost::remove_reference<Type>::type result_type;

      template<typename ChainedPtr>
      Type const &operator()(ChainedPtr const& x) const {
        return operator()(*x);
      }

      Type const &operator()(Class const& x) const {
        return (x.*PtrToMemberFunction)();
      }

      Type const &operator()
        (boost::reference_wrapper<Class const> const& x) const { 
        return operator()(x.get());
      }

      Type const &operator()
        (boost::reference_wrapper<Class> const& x,int=0) const { 
        return operator()(x.get());
      }
    };

#define REF_CONST_MEM_FUN_CONST(Class, Type, MemberFunName)             \
    utils::ref_const_mem_fun_const<Class, Type, &Class::MemberFunName>
  }

  class property_tree {
  public:
    property_tree() {}

    explicit property_tree(std::string const &name)
      : name_(name)
    { }

    std::string const &name() const {
      return name_;
    }

  private:
    typedef boost::multi_index_container<
    property_tree*,
    indexed_by<
      hashed_unique<
        REF_CONST_MEM_FUN_CONST(property_tree, std::string, name)
        >
      >
    > children_t;

    typedef boost::multi_index_container<
      property,
      indexed_by<
        hashed_unique<
          REF_CONST_MEM_FUN_CONST(property, std::string, name)
          >
        >
      > property_t;

    children_t children;
    property_t properties;
    std::string const name_;


    void add_child(property_tree *child) {
      assert(child);
      children.insert(child);
    }

    void add_property(property const &p) {
      properties.insert(p);
    }

    friend void read_config(fs::path const &path, property_tree &);
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

    ~property_tree() {
      for(children_iterator i = children.begin();
          i != children.end();
          ++i)
        delete *i;
    }

  private:
    property_tree(property_tree const &);
    void operator=(property_tree const &);
  };

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

#ifdef STANDALONE
using namespace rest::utils;

void print_tree(property_tree const &ref, unsigned depth = 0) {
  std::string align(depth * 2, ' ');
  std::cout << align << "{ " << ref.name() << "\n";
  for(property_tree::property_iterator i = ref.property_begin();
      i != ref.property_end();
      ++i)
    std::cout << align << "  " << i->name() << " => `"
              << i->data() << "'\n";
  ++depth;
  for(property_tree::children_iterator i = ref.children_begin();
      i != ref.children_end();
      ++i)
    print_tree(**i, depth);
  std::cout << align << "}\n";
}

int main() {
  property_tree t("the_example");
  read_config("/tmp/example", t);
  print_tree(t);
}
#endif
