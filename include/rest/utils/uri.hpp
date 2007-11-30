// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_URI_HPP
#define REST_UTILS_URI_HPP

#include <string>

namespace rest { namespace utils { namespace uri {

std::string escape(std::string::const_iterator, std::string::const_iterator);

inline std::string escape(std::string const &x) {
  return escape(x.begin(), x.end());
}

std::string unescape(
    std::string::const_iterator, std::string::const_iterator, bool form);

inline std::string unescape(std::string const &x, bool form) {
  return unescape(x.begin(), x.end(), form);
}

void make_relative(std::string &uri);

}}}

#endif
