// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_URI_HPP
#define REST_UTILS_URI_HPP

#include <string>

namespace rest { namespace utils { namespace uri {

std::string escape(
  std::string::const_iterator, std::string::const_iterator,
  bool escape_reserved = false);

inline std::string escape(std::string const &x, bool escape_reserved = false) {
  return escape(x.begin(), x.end(), escape_reserved);
}

std::string unescape(
    std::string::const_iterator, std::string::const_iterator, bool form);

inline std::string unescape(std::string const &x, bool form) {
  return unescape(x.begin(), x.end(), form);
}

void make_basename(std::string &uri);

}}}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
