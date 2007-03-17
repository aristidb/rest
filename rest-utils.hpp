// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS
#define REST_UTILS

#include <iosfwd>
#include <string>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/iostreams/pipeline.hpp>
#include <boost/scoped_array.hpp>
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

class boundary_filter : public boost::iostreams::multichar_input_filter {
public:
  boundary_filter(std::string const &boundary)
  : boundary(boundary), buf(new char[boundary.size()]), eof(false),
    pos(boundary.size()) {}
    
  boundary_filter(boundary_filter const &o)
  : boundary(o.boundary), buf(new char[boundary.size()]), eof(false),
    pos(boundary.size()) {}

  template<typename Source>
  std::streamsize read(Source &source, char *outbuf, std::streamsize n) {
    std::streamsize i = 0;
    while (i < n && !eof) {
      std::size_t len = boundary.size() - pos;
      if (boundary.compare(0, len, buf.get() + pos, len) == 0) {
        if (len == boundary.size()) {
          eof = true;
          break;
        }

        if (len > 0)
          std::memmove(buf.get(), buf.get() + pos, len);

        std::streamsize c =
          boost::iostreams::read(source, buf.get() + len, pos);

        if (c == -1) {
          eof = true;
          break;
        } else if (c != pos) {
          pos = boundary.size() - c;
          std::memmove(buf.get() + pos, buf.get(), c);
          continue;
        } else {
          pos = 0;
          int cmp =
              boundary.compare(
                0, boundary.size(),
                buf.get(), boundary.size());
          if (cmp == 0) {
            eof = true;
            break;
          }
        }
      }
      outbuf[i++] = buf[pos++];
    }
    return i ? i : -1;
}

private:
  std::string boundary;
  boost::scoped_array<char> buf;
  bool eof;
  std::streamsize pos;
};
BOOST_IOSTREAMS_PIPABLE(boundary_filter, 0)

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
