// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_CONFIG_HPP
#define REST_UTILS_CONFIG_HPP

#include "rest-utils.hpp"

#include <string>
#include <memory>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

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
    std::string const &data() const;
  };

  class property_tree {
  public:
    property_tree();
    explicit property_tree(std::string const &name);
    ~property_tree();

    std::string const &name() const;

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

    class impl_;
    boost::scoped_ptr<impl_> impl;

    void add_child(property_tree *child);
    void add_property(property const &p);
    friend void read_config(boost::filesystem::path const &path,
                            property_tree &);

  public:
    typedef property_t::const_iterator property_iterator;
    property_iterator property_begin() const;
    property_iterator property_end() const;
    property_iterator find_property(std::string const &name) const;
    
    typedef children_t::const_iterator children_iterator;
    children_iterator children_begin() const;
    children_iterator children_end() const;
    children_iterator find_children(std::string const &name) const;

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

#define ARG_FUN(z, i, _) BOOST_PP_COMMA_IF(i) std::string const &node ## i
#define ARG_NAME(z, i, _) , node ## i

#define DEF_GET_FUN(z, i, _)                                            \
    template<typename T>                                                \
    inline T get(property_tree const &tree, T const &default_value,     \
                 BOOST_PP_REPEAT(i, ARG_FUN, __)) {                     \
      property_tree::children_iterator j = tree.find_children(node0);   \
      if(j == tree.children_end())                                      \
        return default_value;                                           \
      else                                                              \
        return get(**j, default_value                                   \
                   BOOST_PP_REPEAT_FROM_TO(1, i, ARG_NAME, ___));       \
    }

  BOOST_PP_REPEAT_FROM_TO(2, 10, DEF_GET_FUN, _)

#undef DEF_GET_FUN
#undef ARG_NAME
#undef ARG_FUN
}  
  std::auto_ptr<utils::property_tree> config(int argc, char **argv);
}

#endif
