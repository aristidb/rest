// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/scheme.hpp"

using rest::scheme;
using rest::http_scheme;

std::string const &scheme::type_name() {
  static std::string x("scheme");
  return x;
}

bool scheme::need_load_standard_objects = true;

void scheme::load_standard_objects(object_registry &obj_reg) {
  std::auto_ptr<object> http_o(new http_scheme);
  obj_reg.add(http_o);

  need_load_standard_objects = false;
}

scheme::~scheme() {}


std::string const &http_scheme::name() const {
  static std::string x("http");
  return x;
}

rest::object::name_list_type const &http_scheme::name_aliases() const {
  static name_list_type x;
  return x;
}

http_scheme::~http_scheme() {}
