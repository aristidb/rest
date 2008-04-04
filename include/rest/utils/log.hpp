// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

class record_logger;

class logger {
public:
  virtual ~logger();

  record_logger *create_record();

protected:
  virtual record_logger *do_create_record() = 0;
};

class record_logger {
public:
  virtual ~record_logger();

  template<class T>
  void add(std::string const &field, T const &value) {
    std::ostringstream o;
    o << value;
    add(field, o.str());
  }

  void add(std::string const &field, std::string const &value);

  void flush();

  void close();

protected:
  virtual void do_add(std::string const &field, std::string const &value) = 0;
  virtual void do_flush() = 0;
};

}}

#endif
