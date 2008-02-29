// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/identity.hpp>
#include <limits>

using rest::encodings::identity;

std::string identity::name() const {
  return "identity";
}

std::vector<std::string> identity::aliases() const {
  std::vector<std::string> v;
  v.push_back("");
  return v;
}

bool identity::is_identity() const {
  return true;
}

int identity::priority() const {
  return std::numeric_limits<int>::max(); // the lowest priority
}

void identity::add_reader(input_chain &) {}

void identity::add_writer(output_chain &) {}
