// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>
#include <map>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

class log_record;

class logger {
public:
  virtual ~logger() {}

  virtual void log(log_record const &) = 0;
};

class log_record {
public:
  template<class T>
  void add(std::string const &field, T const &value) {
    std::ostringstream o;
    o << value;
    add(field, o.str());
  }

  void add(std::string const &field, std::string const &value) {
    fields[field] = value;
  }

private:
  std::map<std::string, std::string> fields;
};

}}

#endif
