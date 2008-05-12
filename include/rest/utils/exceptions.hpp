// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_EXCEPTIONS_HPP
#define REST_UTILS_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>
#include <string.h>
#include <errno.h>

namespace rest { namespace utils {

class error 
  : public std::runtime_error
{
public:
  error(std::string const &s)
    : std::runtime_error(s)
  { }
};

class errno_error
  : public error
{
public:
  errno_error(std::string const &s)
    : error(s + ": `" +  ::strerror(errno) + "'")
  { }
};

}}

#endif
