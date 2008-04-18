// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_OBJECT_HPP
#define REST_OBJECT_HPP

#include <string>
#include <vector>
#include <memory>
#include <boost/scoped_ptr.hpp>

namespace rest {

class object {
public:
  typedef std::vector<std::string> name_list_type;

  virtual ~object() = 0;

  virtual std::string const &type() const = 0;
  virtual std::string const &name() const = 0;
  virtual std::vector<std::string> const &name_aliases() const = 0;
};

class object_registry {
public:
  static object_registry &get();

  void add(std::auto_ptr<object> &);
  object *find(std::string const &type, std::string const &name) const;

private:
  object_registry();
  ~object_registry();

  //dummy
  object_registry(object_registry const &);
  object_registry &operator=(object_registry const &);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
