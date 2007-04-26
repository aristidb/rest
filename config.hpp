// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_CONFIG_HPP
#define REST_UTILS_CONFIG_HPP

#include "rest-utils.hpp"

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

using namespace boost::multi_index;

namespace rest {
namespace utils {
template<class Class,typename Type,
         Type const &(Class::*PtrToMemberFunction)() const>
class ref_const_mem_fun_const {
public:
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

    std::string const &name() const;
  private:
    typedef boost::multi_index_container<
    	property_tree*,
      indexed_by<
        hashed_unique<
          ref_const_mem_fun_const<property_tree, std::string,
                                  &property_tree::name>
        >
      >
    > children_t;

    typedef boost::multi_index_container<
      property,
      indexed_by<
        hashed_unique<
          ref_const_mem_fun_const<property, std::string,
                                   &property::name
          >
        >
      >
    > property_t;

    class impl_;
    boost::scoped_ptr<impl_> impl;

    void add_child(property_tree *child);
    void add_property(property const &p);
    friend void read_config(fs::path const &path, property_tree &);
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
}}

#endif
