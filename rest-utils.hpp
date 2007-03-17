// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS
#define REST_UTILS

#include <iosfwd>
#include <string>
#include <boost/iostreams/concepts.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>

namespace rest { namespace utils {

namespace uri {
  std::string escape(std::string::const_iterator, std::string::const_iterator);

  inline std::string escape(std::string const &x) {
    return escape(x.begin(), x.end());
  }

  std::string unescape(
      std::string::const_iterator, std::string::const_iterator, bool form);

  inline std::string unescape(std::string const &x, bool form) {
    return unescape(x.begin(), x.end(), form);
  }
}

class boundary_reader : public boost::iostreams::source {
public:
  boundary_reader(std::istream &source, std::string const &boundary);
  boundary_reader(boundary_reader const &);
  ~boundary_reader();

  void swap(boundary_reader &o) {
    p.swap(o.p);
  }

  boundary_reader &operator=(boundary_reader o) {
    o.swap(*this);
    return *this;
  }

  std::streamsize read(char *s, std::streamsize n);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

class logger : boost::noncopyable {
public: // thread safe
  static logger &get();

  void set_minimum_priority(int priority);

  void log(int priority, char const *file, int line, std::string const &data);

private:
  logger();
  ~logger();

  static void init();

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

enum { CRITICAL = 100, IMPORTANT = 90, INFO=50, DEBUG = 0 };

#define REST_LOG(prio, data) \
  ::rest::logger::get().log((prio), __FILE__, __LINE__, (data))

}}

#endif
