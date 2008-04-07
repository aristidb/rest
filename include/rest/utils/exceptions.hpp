// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_EXCEPTIONS_HPP
#define REST_UTILS_EXCEPTIONS_HPP

#include <stdexcept>
#include <string.h>
#include <errno.h>

namespace rest { namespace utils {

class errno_error
  : public std::runtime_error
{
public:
  errno_error(std::string const &s)
    : std::runtime_error(s + ": `" +  ::strerror(errno) + "'")
  { }
};

}}

#endif
