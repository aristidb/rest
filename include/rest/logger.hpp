// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>
#include <string>
#include <sstream>
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>

namespace rest {

class logger : boost::noncopyable {
public:
  typedef unsigned short sequence_number_type;

  enum priority {
    debug    = -1000,
    info     =     0,
    notice   =   100,
    warning  =   200,
    err      =   300,
    critical =  1000,
    alert    =  1500,
    emerg    =  2000
  };

  logger(priority min_priority)
  : min_priority(min_priority), sequence_number(0U) {}

  virtual ~logger() {}

  void set_sequence_number(sequence_number_type sequence_number) {
    flush();
    this->sequence_number = sequence_number;
  }

  void next_sequence_number() {
    flush();
    if (++sequence_number == 0)
      ++sequence_number;
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

  void log(
    priority prio, std::string const &field)
  {
    if (prio >= min_priority)
      do_log(prio, field, std::string());
  }

  void flush() {
    do_flush();
  }

protected:
  sequence_number_type get_sequence_number() const {
    return sequence_number;
  }

protected:
  virtual void do_log(
    priority, std::string const &, std::string const &) = 0;

  virtual void do_flush() = 0;

private:
  priority min_priority;
  sequence_number_type sequence_number;
};

class plaintext_logger : public logger {
public:
  plaintext_logger(priority min_priority);
  ~plaintext_logger();

private:
  void do_log(priority, std::string const &, std::string const &);
  void do_flush();

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

class null_logger : public logger {
public:
  null_logger() : logger(logger::emerg) {}
private:
  void do_log(priority, std::string const &, std::string const&) {}
  void do_flush() {}
};

}

#endif
