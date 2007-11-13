// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_STRING_HPP
#define REST_UTILS_STRING_HPP

#include <string>
#include <algorithm>
#include <cctype>

namespace rest { namespace utils {

struct string_icompare {
  bool operator() (std::string const &a, std::string const &b) const {
    return std::lexicographical_compare(
        a.begin(), a.end(),
        b.begin(), b.end(),
        *this);
  }

  bool operator() (char a, char b) const {
    return std::tolower(a) < std::tolower(b);
  }
};

}}

#endif
