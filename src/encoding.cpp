// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encoding.hpp>
#include <rest/encodings/identity.hpp>

using rest::encoding;

std::string const &encoding::type_name() {
  static std::string x("encoding");
  return x;
}

bool encoding::need_load_standard_objects = true;

void encoding::load_standard_objects(object_registry &obj_reg) {
  std::auto_ptr<object> identity_o(new rest::encodings::identity);
  obj_reg.add(identity_o);

  need_load_standard_objects = false;
}

encoding::~encoding() {}
