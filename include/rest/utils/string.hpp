// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_STRING_HPP
#define REST_UTILS_STRING_HPP

#include <string>
#include <algorithm>
#include <cctype>
#include <boost/functional/hash.hpp>
#include <boost/iostreams/write.hpp>

namespace rest { namespace utils {

struct string_ihash {
  std::size_t operator() (std::string const &x) const {
    std::size_t seed = 0;
    for (std::string::const_iterator it = x.begin(); it != x.end(); ++it)
      boost::hash_combine(seed, std::tolower(*it));
    return seed;
  }
};

struct string_iequals {
  bool operator() (std::string const &a, std::string const &b) const {
    if (a.size() != b.size())
      return false;
    return std::equal(a.begin(), a.end(), b.begin(), *this);
  }

  bool operator() (char a, char b) const {
    return std::tolower(a) == std::tolower(b);
  }
};

template<typename Sink>
void write_string(Sink &sink, std::string const &str) {
  boost::iostreams::write(sink, str.data(), str.size());
}

}}

#endif
