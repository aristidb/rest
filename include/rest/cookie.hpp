// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_COOKIE_HPP
#define REST_COOKIE_HPP

#include <string>
#include <time.h>

namespace rest {

struct cookie {
  std::string name;
  std::string value;

  std::string domain;
  time_t expires;
  std::string path;
  bool secure;

  cookie(std::string const &name, std::string const &value,
         time_t expires = time_t(-1), std::string const &path = "",
         std::string const &domain = "")
    : name(name), value(value), domain(domain),
      expires(expires), path(path), secure(false)
  { }
};

}

#endif
