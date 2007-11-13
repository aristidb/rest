// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_CACHE_HPP
#define REST_CACHE_HPP

#include <string>

namespace rest { namespace cache {

enum flags {
  NO_FLAGS = 0U,
  private_ = 1U,
  no_cache = 2U,
  no_store = 4U,
  no_transform = 8U
};

inline flags operator~(flags x) {
  return flags(~(unsigned)x);
}

inline flags operator|(flags a, flags b) {
  return flags((unsigned)a | (unsigned)b);
}

inline flags &operator|=(flags &a, flags b) {
  return a = (a | b);
}

inline flags operator&(flags a, flags b) {
  return flags((unsigned)a & (unsigned)b);
}

inline flags &operator&=(flags &a, flags b) {
  return a = (a & b);
}

inline flags default_header_flags(std::string const &header) {
  if (header == "set-cookie" || header == "set-cookie2")
    return no_cache;
  return NO_FLAGS;
}

}}

#endif
