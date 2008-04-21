// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SCHEME_HPP
#define REST_SCHEME_HPP

#include "object.hpp"

namespace rest {

class scheme : public object {
public:
  static std::string const &type_name();

  static bool need_load_standard_objects;
  static void load_standard_objects(object_registry &obj_reg);

  std::string const &type() const { return type_name(); }

  virtual ~scheme() = 0;
};

class http_scheme : public scheme {
public:
  std::string const &name() const;
  name_list_type const &name_aliases() const;

  ~http_scheme();
};

}

#endif
