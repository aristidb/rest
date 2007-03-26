// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS
#define REST_UTILS

#include <iosfwd>
#include <string>
#include <sstream>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/iostreams/pipeline.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
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

inline boost::tuple<bool,int> hex2int(int ascii) {
  if(std::isdigit(ascii))
    return boost::make_tuple(true, ascii - '0');
  else if(ascii >= 'a' && ascii <= 'f')
    return boost::make_tuple(true, ascii - 'a' + 0xa);
  else if(ascii >= 'A' && ascii <= 'F')
    return boost::make_tuple(true, ascii - 'A' + 0xA);
  else
    return boost::make_tuple(false, 0);
}

struct socket_device {
  typedef char char_type;
  typedef boost::iostreams::bidirectional_device_tag category;

  socket_device(int fd, long timeout);
  ~socket_device();

  std::streamsize read(char *, std::streamsize);
  std::streamsize write(char const *, std::streamsize);

private:
  class impl;
  boost::shared_ptr<impl> p;
};

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

class length_filter : public boost::iostreams::multichar_input_filter {
public:
  length_filter(std::streamsize length) : length(length) {}

  template<typename Source>
  std::streamsize read(Source &source, char *outbuf, std::streamsize n) {
    namespace io = boost::iostreams;

    std::streamsize c;
    if (n <= length)
      c = io::read(source, outbuf, n);
    else
      c = io::read(source, outbuf, length);
    if (c == -1)
      return -1;
    length -= c;
    return c;
  }

private:
  std::streamsize length;
};
BOOST_IOSTREAMS_PIPABLE(length_filter, 0)

class chunked_filter : public boost::iostreams::multichar_input_filter {
public:
  chunked_filter() : pending(0) {}

  template<typename Source>
  std::streamsize read(Source &source, char *outbuf, std::streamsize n) {
    namespace io = boost::iostreams;

    if (pending == -1)
      return -1;
    else if (pending == 0) {
      int c = io::get(source);
      if(c == Source::traits_type::eof())
        return -1;
      else if(c == '\r') {
        c = io::get(source);
        if(c != '\n')
         return -1;
        c = io::get(source);
        if(c == Source::traits_type::eof())
          return -1;
      }
      for(;;) {
        boost::tuple<bool, int> value = hex2int(c);
        if(value.get<0>()) {
          pending *= 0x10;
          pending += value.get<1>();
        }
        else
          break;
        c = io::get(source);
        if(c == Source::traits_type::eof())
          return -1;
      }
      bool cr = false;
      for(;;) {
        if(c == '\r')
          cr = true;
        else if(cr && c == '\n')
          break;
        else if(c == Source::traits_type::eof())
          return -1;
        c = io::get(source);
      }
      if(pending == 0) {
        pending = -1;
        return -1;
      }
    }
    std::streamsize c;
    if (n <= pending)
      c = io::read(source, outbuf, n);
    else
      c = io::read(source, outbuf, pending);
    if (c == -1)
      return -1;
    pending -= c;
    return c;
  }

private:
  std::streamsize pending;
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
  ::rest::utils::logger::get().log((prio), __FILE__, __LINE__, (data))

#define REST_LOG_E(prio, data) \
  do { \
    ::std::stringstream sstr; sstr << data;                               \
    ::rest::utils::logger::get().log((prio), __FILE__, __LINE__, sstr.str()); \
  } while (0)

#define REST_LOG_ERRNO(prio, data) \
  do { \
   ::std::stringstream sstr;                 \
   sstr << data << ": `" << ::std::strerror(errno) << '\''; \
   ::rest::utils::logger::get().log((prio), __FILE__, __LINE__, sstr.str()); \
  } while (0)

std::string current_date_time();
}}

#endif
