// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encoding.hpp>

using rest::encoding;
using rest::encodings_registry;

encoding::~encoding() {}

encodings_registry &encodings_registry::get() {
  static encodings_registry reg;
  return reg;
}

encodings_registry::encodings_registry() {
}

encoding *encodings_registry::find_encoding(std::string const &) const {
  return 0;
}
