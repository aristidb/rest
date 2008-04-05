// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>
#include <vector>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

struct log_record;

class logger {
public:
  virtual ~logger() {}

  virtual void log(log_record const &) = 0;
};

enum log_priority {
  LOG_NORMAL
};

struct log_record {
  typedef std::pair<std::string, std::string> field_type;
  typedef std::vector<field_type> fields_type;

  fields_type fields;
  log_priority priority;

  log_record(log_priority priority = LOG_NORMAL)
  : priority(priority)
  {}

  template<class T>
  void add_field(std::string const &field, T const &value) {
    std::ostringstream o;
    o << value;
    fields.push_back(field_type(field, o.str()));
  }
};

}}

#endif
