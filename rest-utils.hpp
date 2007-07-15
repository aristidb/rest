// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS
#define REST_UTILS

#include <map>
#include <set>
#include <cerrno>
#include <vector>
#include <iosfwd>
#include <string>
#include <sstream>
#include <stdexcept>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/pipeline.hpp>
#include <boost/iostreams/flush.hpp>
#include <boost/iostreams/write.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/current_function.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>
#include <boost/format.hpp>
#include <boost/ref.hpp>

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

  struct category
    :
      boost::iostreams::bidirectional_device_tag,
      boost::iostreams::closable_tag
  {};

  socket_device(int fd, long timeout);
  ~socket_device();

  void close(std::ios_base::open_mode);

  void push_cork();
  void pull_cork();

  std::streamsize read(char *, std::streamsize);
  std::streamsize write(char const *, std::streamsize);

private:
  class impl;
  boost::shared_ptr<impl> p;
};

// TODO: speed
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
    if (eof)
      return -1;
    std::streamsize i = 0;
    while (i < n && !eof) {
      std::size_t len = boundary.size() - pos;
      if (boundary.compare(0, len, buf.get() + pos, len) == 0) {
        if (len == boundary.size()) {
          eof = true;
          break;
        }

        if (len > 0)
          memmove(buf.get(), buf.get() + pos, len);

        std::streamsize c =
          boost::iostreams::read(source, buf.get() + len, pos);

        if (c == -1) {
          eof = true;
          break;
        } else if (c != pos) {
          pos = boundary.size() - c;
          memmove(buf.get() + pos, buf.get(), c);
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
    if (eof)
      skip_transport_padding(source);
    return i ? i : -1;
  }

  template<typename Source>
  void skip_transport_padding(Source &source) {
    int ch;
    // skip LWS
    while ((ch = boost::iostreams::get(source)) == ' ' && ch == '\t')
      ;
    if (ch == EOF)
      return;
    // "--" indicates end-of-streams
    if (ch == '-' && boost::iostreams::get(source) == '-') {
      // soak up all the content, it is to be ignored
      while (boost::iostreams::get(source) != EOF)
        ;
      return;
    }
    // expect CRLF
    boost::iostreams::putback(source, ch);
    if ((ch = boost::iostreams::get(source)) != '\r') {
      if (ch == EOF)
        return;
      boost::iostreams::putback(source, ch);
    }
    if ((ch = boost::iostreams::get(source)) != '\n') {
      if (ch == EOF)
        return;
      boost::iostreams::putback(source, ch);
    }
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

class chunked_filter {
public:
  typedef char char_type;

  struct category
    :
      boost::iostreams::filter_tag,
      boost::iostreams::multichar_tag,
      boost::iostreams::dual_use,
      boost::iostreams::closable_tag
  {};

  chunked_filter() : pending(0) { }

  template<typename Sink>
  std::streamsize write(Sink& snk, char const* s, std::streamsize n) {
    namespace io = boost::iostreams;

    assert(n >= 0);
    boost::format length("%1$x");
    length % n;

    // TODO handle 0 <= ret < length 

    std::streamsize ret = io::write(snk, length.str().c_str(),
                                    length.str().length());
    if(ret < 0)
      return ret;
    if( (ret = io::write(snk, "\r\n", 2)) < 0 )
      return ret;
    if( (ret = io::write(snk, s, n)) < 0 )
      return ret;
    if( (ret = io::write(snk, "\r\n", 2)) < 0)
      return ret;

    return n;
  }

  template<typename Device>
  void close(Device &d, std::ios_base::open_mode mode) {
    namespace io = boost::iostreams;
    if (mode & std::ios_base::out) {
      io::write(d, "0\r\n\r\n", 5);
      io::flush(d);
    }
  }


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

  void log(int priority, char const *file, char const *func,
           int line, std::string const &data);

private:
  logger();
  ~logger();

  static void init();

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

enum { CRITICAL = 100, IMPORTANT = 90, INFO=50, DEBUG = 0 };

#define REST_LOG(prio, data)                           \
    ::rest::utils::logger::get().log((prio), __FILE__, \
    BOOST_CURRENT_FUNCTION, __LINE__, (data))

#define REST_LOG_E(prio, data)                                          \
  do {                                                                  \
    ::std::stringstream sstr; sstr << data;                             \
    ::rest::utils::logger::get().log((prio), __FILE__,                  \
    BOOST_CURRENT_FUNCTION, __LINE__, sstr.str());                      \
  } while (0)

#define REST_LOG_ERRNO(prio, data)                       \
  do {                                                   \
   ::std::stringstream sstr;                             \
   sstr << data << ": `" << ::strerror(errno) << '\'';   \
   ::rest::utils::logger::get().log((prio), __FILE__,    \
    BOOST_CURRENT_FUNCTION, __LINE__, sstr.str());       \
  } while (0)

namespace http {
  struct remote_close {};
  struct bad_format {};

  std::string current_date_time();

  // checks if `c' is a space or a h-tab (see RFC 2616 chapter 2.2)
  inline bool isspht(char c) {
    return c == ' ' || c == '\t';
  }

  template<class Source, typename Char>
  bool expect(Source &in, Char c) {
    int t = boost::iostreams::get(in);
    if(t == c)
      return true;
    else if(t == EOF)
      throw remote_close();

    boost::iostreams::putback(in, t);
    return false;
  }

  template<class Source>
  int remove_spaces(Source &in) {
    int c;
    do {
      c = boost::iostreams::get(in);
    } while(isspht(c));
    boost::iostreams::putback(in, c);
    return c;
  }

  typedef boost::tuple<std::string, std::string, std::string> request_line;
  enum { REQUEST_METHOD, REQUEST_URI, REQUEST_HTTP_VERSION };

  template<class Source>
  void get_until(char end, Source &in, std::string &ret) {
    int t;
    while ((t = boost::iostreams::get(in)) != end) {
      if(t == EOF)
        throw remote_close();
      ret += t;
    }
  }

  template<class Source>
  request_line get_request_line(Source &in) {
    request_line ret;
    get_until(' ', in, ret.get<REQUEST_METHOD>());
    get_until(' ', in, ret.get<REQUEST_URI>());
    get_until('\r', in, ret.get<REQUEST_HTTP_VERSION>());
    if(!expect(in, '\n'))
      throw bad_format();
    return ret;
  }

  typedef std::map<std::string, std::string> header_fields;

  namespace detail {
    enum {
      CSV_HEADERS=4
    };
    static char const * const csv_header[CSV_HEADERS] = {
      "accept", "accept-charset", "accept-encoding",
      "accept-language"   // TODO ...
    };
    static char const * const * const csv_header_end = csv_header + CSV_HEADERS;
  }

  // reads a header field from `in' and adds it to `fields'
  // see RFC 2616 chapter 4.2
  // Warning: Field names are converted to all lower-case!
  template<class Source>
  std::pair<header_fields::iterator, bool> 
  get_header_field(Source &in, header_fields &fields) {
    namespace io = boost::iostreams;
    std::string name;
    int t = 0;
    for (;;) {
      t = io::get(in);
      if (t == '\n' || t == '\r')
        throw bad_format();
      else if (t == EOF)
        throw remote_close();
      else if (t == ':') {
        remove_spaces(in);
        break;
      }
      else
        name += std::tolower(t);
    }
    std::string value;
    for(;;) {
      t = io::get(in);
      if(t == '\n' || t == '\r') {
        // Newlines in header fields are allowed when followed
        // by an SP (space or horizontal tab)
        if(t == '\r')
          expect(in, '\n');
        t = io::get(in);
        if (!isspht(t)) {
          io::putback(in, t);
          break;
        }
        remove_spaces(in);
        value += ' ';
      }
      else if (t == EOF)
        throw remote_close();
      else
        value += t;
    }
    std::pair<header_fields::iterator, bool> ret =
      fields.insert(std::make_pair(name, value));
    if (!ret.second && 
        std::find(detail::csv_header, detail::csv_header_end, name)
          != detail::csv_header_end)
      fields[name] += std::string(", ") + value;
    return ret;
  }

  template<class Source>
  header_fields read_headers(Source &source) {
    header_fields fields;
    do {
      get_header_field(source, fields);
    } while (!(expect(source, '\r') && expect(source, '\n')));
    return fields;
  }

  void parse_parametrised(
      std::string const &in,
      std::string &element,
      std::set<std::string> const &interesting_parameters,
      std::map<std::string, std::string> &parameters);

  void parse_list(std::string const &in, std::vector<std::string> &out);

  void parse_qlist(std::string const &in, std::multimap<int, std::string> &out);

  int parse_qvalue(std::string const &in);
}

class errno_error
  : public std::runtime_error
{
public:
  errno_error(std::string const &s)
    : std::runtime_error(s + ": `" +  ::strerror(errno) + "'")
  { }
};

template<class Class,typename Type,
         Type const &(Class::*PtrToMemberFunction)() const>
class ref_const_mem_fun_const {
public:
  typedef typename boost::remove_reference<Type>::type result_type;

  template<typename ChainedPtr>
  Type const &operator()(ChainedPtr const& x) const {
    return operator()(*x);
  }

  Type const &operator()(Class const& x) const {
    return (x.*PtrToMemberFunction)();
  }

  Type const &operator()
    (boost::reference_wrapper<Class const> const& x) const { 
    return operator()(x.get());
  }

  Type const &operator()
    (boost::reference_wrapper<Class> const& x,int=0) const { 
    return operator()(x.get());
  }
};

struct end { };
}}

#endif
