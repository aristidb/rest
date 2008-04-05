// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>
#include <vector>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

typedef std::vector<std::pair<std::string, std::string> > log_record;

class logger {
public:
  virtual ~logger() {}

  virtual void log(log_record const &) = 0;
};

template<class T>
void add_field(log_record &rec, std::string const &field, T const &value) {
  std::ostringstream o;
  o << value;
  rec.push_back(std::make_pair(field, o.str()));
}

}}

#endif
