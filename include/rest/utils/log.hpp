// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>
#include <vector>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

class logger {
public:
  typedef unsigned short running_number_type;

  enum priority {
    debug = -1000,
    normal
  };

  logger(priority min_priority)
  : min_priority(min_priority) {}

  virtual ~logger() {}

  void set_running_number(running_number_type running_number) {
    this->running_number = running_number;
  }

  void set_minimum_priority(priority min_priority) {
    this->min_priority = min_priority;
  }

  void log(
    priority prio, std::string const &field, std::string const &value)
  {
    if (prio >= min_priority) {
      do_log(prio, field, value);
    }
  }

  template<class T>
  void log(
    priority prio, std::string const &field, T const &value)
  {
    if (prio >= min_priority) {
      std::ostringstream o;
      o << value;
      do_log(prio, field, o.str());
    }
  }

  void flush() {
    do_flush();
  }

protected:
  running_number_type get_running_number() const {
    return running_number;
  }

protected:
  virtual void do_log(
    priority, std::string const &, std::string const &) = 0;

  virtual void do_flush() = 0;

private:
  priority min_priority;
  running_number_type running_number;
};

}}

#endif
