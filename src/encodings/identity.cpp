// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/identity.hpp>
#include <limits>

using rest::encodings::identity;

std::string const &identity::name() const {
  static std::string x("identity");
  return x;
}

rest::object::name_list_type const &identity::name_aliases() const {
  static name_list_type x(1, "");
  return x;
}

bool identity::is_identity() const {
  return true;
}

int identity::priority() const {
  return std::numeric_limits<int>::max(); // the lowest priority
}

void identity::add_reader(input_chain &) {}

void identity::add_writer(output_chain &) {}
