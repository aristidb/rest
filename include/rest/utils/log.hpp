// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>
#include <vector>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

enum log_priority {
  log_debug = -1000,
  log_normal
};

struct log_record {
  typedef std::pair<std::string, std::string> field_type;
  typedef std::vector<field_type> fields_type;

  fields_type fields;
  log_priority priority;

  log_record(log_priority priority = log_normal)
  : priority(priority)
  {}

  template<class T>
  void add_field(std::string const &field, T const &value) {
    std::ostringstream o;
    o << value;
    fields.push_back(field_type(field, o.str()));
  }
};

class logger {
public:
  logger(log_priority min_priority)
  : min_priority(min_priority) {}

  virtual ~logger() {}

  void set_minimum_priority(log_priority min_priority) {
    this->min_priority = min_priority;
  }

  void log(log_record const &rec) {
    if (rec.priority >= min_priority) {
      do_log(rec);
    }
  }

protected:
  virtual void do_log(log_record const &) = 0;

private:
  log_priority min_priority;
};

}}

#endif
