// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

class logger {
public:
  virtual ~logger() {}

  virtual void start_request(int id) = 0;
  virtual void end_request() = 0;

  template<class T>
  void add(std::string const &field, T const &value) {
    std::ostringstream o;
    o << value;
    add(field, o.str());
  }

  virtual void add(std::string const &field, std::string const &value) = 0;

  virtual void flush() = 0;
};

}}

#endif
