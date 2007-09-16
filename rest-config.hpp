// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_CONFIG_HPP
#define REST_UTILS_CONFIG_HPP

#include "rest-utils.hpp"

#include <string>
#include <memory>
#include <cassert>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/preprocessor/punctuation/paren.hpp>
#include <boost/preprocessor/facilities/expand.hpp>
#include <boost/preprocessor/comparison/greater.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/preprocessor/cat.hpp>

namespace rest {
namespace utils {  
  class property {
    class impl;
    boost::shared_ptr<impl> impl_;
  public:
    property(std::string const &name,
             boost::filesystem::path const &file);

    property(std::string const &name, std::string const &data);

    std::string const &name() const;
    void name(std::string const &n);
    std::string const &data() const;
    void data(std::string const &d);
  };

  class property_tree {
  public:
    property_tree() { }
    explicit property_tree(std::string const &name) : name_(name) { }
    ~property_tree();

    std::string const &name() const { return name_; }
  private:
    typedef boost::multi_index_container<
      property_tree*,
      boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
          ref_const_mem_fun_const<property_tree, std::string,
                                  &property_tree::name>
        >
      >
    > children_t;

    typedef boost::multi_index_container<
        property,
        boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<
            ref_const_mem_fun_const<property, std::string,
                                    &property::name>
          >
        >
      > property_t;

    children_t children;
    property_t properties;
    std::string const name_;
  public:
    bool add_child(property_tree *child) {
      assert(child);
      typedef std::pair<children_t::iterator, bool> result_t;
      result_t r = children.insert(child);
      return r.second;
    }
    void add_property(property const &p) {
      properties.insert(p);
    }

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

    template<typename Modifier>
    bool modify_property(property_iterator i, Modifier m) {
      return properties.modify(i, m);
    }
    bool replace_property(property_iterator i, property const &p) {
      return properties.replace(i, p);
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
  private:
    property_tree(property_tree const &);
    void operator=(property_tree const &);
  };

  template<typename T>
  inline T get(property_tree const &tree, T const &default_value,
               std::string const &node0)
  {
    property_tree::property_iterator i = tree.find_property(node0);
    if(i == tree.property_end())
      return default_value;
    else {
      std::stringstream str(i->data());
      T ret;
      str >> ret;
      return ret;
    }
  }

  inline std::string const &get(property_tree const &tree,
                                std::string const &default_value,
                                std::string const &node0)
  {
    property_tree::property_iterator i = tree.find_property(node0);
    if(i == tree.property_end())
      return default_value;
    else
      return i->data();
  }

  inline property_tree::children_iterator
  get(property_tree const &tree,
      property_tree::children_iterator const &default_value,
      std::string const &node0)
  {
    property_tree::children_iterator i = tree.find_children(node0);
    if(i == tree.children_end())
      return default_value;
    else
      return i;
  }

#define ARG_FUN(z, i, _) , std::string const &node ## i
#define ARG_NAME(z, i, _) , node ## i

#define DEF_GET_FUN(z, i, _)                                            \
    template<typename T>                                                \
    inline T get(                                                       \
        property_tree const &tree,                                      \
        T const &default_value                                          \
        BOOST_PP_REPEAT(i, ARG_FUN, __))                                \
    {                                                                   \
      property_tree::children_iterator j = tree.find_children(node0);   \
      if(j == tree.children_end())                                      \
        return default_value;                                           \
      else                                                              \
        return get(**j, default_value                                   \
                   BOOST_PP_REPEAT_FROM_TO(1, i, ARG_NAME, ___));       \
    }

  BOOST_PP_REPEAT_FROM_TO(2, 10, DEF_GET_FUN, _)

  inline property_tree &add_path(
    property_tree &tree, std::string const &node0)
  {
    property_tree::children_iterator j = tree.find_children(node0);
    if(j == tree.children_end()) {
      property_tree *child = new property_tree(node0);
      tree.add_child(child);
      return *child;
    }
    return **j;
  }

#define DEF_ADD_PATH_FUN(z, i, _)                                             \
    inline property_tree &add_path(                                           \
        property_tree &tree                                                   \
        BOOST_PP_REPEAT(i, ARG_FUN, ~))                                       \
    {                                                                         \
      property_tree::children_iterator j = tree.find_children(node0);         \
      if(j == tree.children_end()) {                                          \
        property_tree *child = new property_tree(node0);                      \
        tree.add_child(child);                                                \
        return add_path(*child BOOST_PP_REPEAT_FROM_TO(1, i, ARG_NAME, ~));   \
      } else {                                                                \
        return add_path(**j BOOST_PP_REPEAT_FROM_TO(1, i, ARG_NAME, ~));      \
      }                                                                       \
    }                                                                         \
    /**/

  BOOST_PP_REPEAT_FROM_TO(2, 10, DEF_ADD_PATH_FUN, _)

  namespace {
    class data_setter {
      std::string const &new_data;
    public:
      data_setter(std::string const &new_data) : new_data(new_data) { }

      void operator()(property &p) const {
        p.data(new_data);
      }
    };
  }

  inline void set(
      property_tree &tree,
      std::string const &value,
      std::string const &node0)
  {
    property_tree::property_iterator j = tree.find_property(node0);
    if(j == tree.property_end())
      tree.add_property(property(node0, value));
    else {
      //property p = *j;
      //p.data(value);
      //tree.replace_property(j, p);
      tree.modify_property(j, data_setter(value));
    }
  }

  template<typename T>
  inline void set(
    property_tree &tree, T const &value, std::string const &node0)
  {
    std::stringstream sstr;
    sstr << value;
    rest::utils::set(tree, sstr.str(), node0);
  }

#define DEF_SET_FUN(z, i, _)                                                   \
    template<typename T>                                                       \
    inline void set(                                                           \
        property_tree &tree,                                                   \
        T const &value                                                         \
        BOOST_PP_REPEAT(i, ARG_FUN, ~))                                        \
    {                                                                          \
      property_tree::children_iterator j = tree.find_children(node0);          \
      if(j == tree.children_end()) {                                           \
        property_tree &child = add_path(                                       \
            tree                                                               \
            BOOST_PP_REPEAT_FROM_TO(                                           \
              0,                                                               \
              BOOST_PP_DEC(i),                                                 \
              ARG_NAME,                                                        \
              ~                                                                \
            )                                                                  \
          );                                                                   \
        rest::utils::set(child, value, BOOST_PP_CAT(node, BOOST_PP_DEC(i)));   \
      }                                                                        \
      else {                                                                   \
        rest::utils::set(**j, value                                            \
           BOOST_PP_REPEAT_FROM_TO(1, i, ARG_NAME, ~));                        \
      }                                                                        \
    }                                                                          \
    /**/

  BOOST_PP_REPEAT_FROM_TO(2, 10, DEF_SET_FUN, _)

#undef DEF_SET_FUN
#undef DEF_ADD_PATH_FUN
#undef DEF_GET_FUN
#undef ARG_NAME
#undef ARG_FUN
}

  class config {
  public:
    static config &get();
    void load(int argc, char **argv, char const *config_path = 0x0, 
              bool allow_params = true);
    utils::property_tree &tree();

  private:
    config();

    //DUMMY
    config(config const &);
    config &operator =(config const &);

    utils::property_tree tree_;
  };
}

#endif
