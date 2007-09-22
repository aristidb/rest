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

#include <syslog.h>

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

  socket_device(int fd, long timeout_rd, long timeout_wr);
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

class boundary_filter : public boost::iostreams::multichar_input_filter {
public:
  boundary_filter(std::string const &boundary)
  : boundary(boundary), buf(new char[boundary.size()]), eof(false),
    pos(boundary.size())
  {
    kmp_init();
  }
    
  boundary_filter(boundary_filter const &o)
  : boundary(o.boundary), buf(new char[boundary.size()]), eof(false),
    pos(boundary.size())
  {
    kmp_init();
  }

private:
  struct eof_event {};

public:
  template<typename Source>
  std::streamsize read(Source &source, char *outbuf, std::streamsize n_) {
    if (eof)
      return -1;
    if (n_ <= 0)
      return 0;
    std::size_t n = std::size_t(n_);
    std::size_t i = 0;
    try {
      while (i < n) {
        std::size_t c = update(source) - pos;
        if (c > n - i)
          c = n - i;
        memcpy(outbuf + i, buf.get() + pos, c);
        i += c;
        pos += c;
      }
    } catch (eof_event&) {
      skip_transport_padding(source);
      eof = true;
    }
    return i ? i : -1;
  }

private:
  template<typename Source>
  std::size_t update(Source &source) {
    namespace io = boost::iostreams;

    bool end = false;

    std::size_t x;

    while ((x = check_boundary()) == pos)
    {
      if (end || pos == 0)
        throw eof_event();

      memmove(buf.get(), buf.get() + pos, boundary.size() - pos);

      do {
        std::streamsize c = 
          io::read(source, buf.get() + boundary.size() - pos, pos);

        if (c < 0)
          break;

        pos -= c;
      } while (pos > 0);
      
      if (pos != 0) {
        end = true;
        memmove(buf.get() + pos, buf.get(), boundary.size() - pos);
      }
    }

    return x;
  }

  void kmp_init() {
    kmp_next.resize(boundary.size() + 1);
    std::size_t i = 0;
    int j = -1;
    kmp_next[0] = -1;
    while (i < boundary.size()) {
      while (j >= 0 && boundary[j] != boundary[i])
        j = kmp_next[j];
      ++i;
      ++j;
      kmp_next[i] = j;
    }
  }

  std::size_t check_boundary() {
    std::size_t i = pos;
    int j = 0;
    std::size_t a = i;
    while (i < boundary.size()) {
      while (j >= 0 && buf[i] != boundary[j])
        j = kmp_next[j];
      ++i;
      ++j;
      if (j <= 0)
        a = i;
    }
    return a;
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
  std::size_t pos;
  std::vector<int> kmp_next;
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

    if (n == 0)
      return 0;

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

void log(int priority, char const *message, ...);

namespace http {
  struct remote_close {};
  struct bad_format {};

  std::string datetime_string(time_t);
  time_t datetime_value(std::string const &);

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
  void get_until(char end, Source &in, std::string &ret,
                 bool allow_cr_or_lf = false) {
    int t;
    while ((t = boost::iostreams::get(in)) != end) {
      if(t == EOF)
        throw remote_close();
      if (!allow_cr_or_lf && (t == '\r' || t == '\n'))
        throw bad_format();
      ret += t;
    }
  }

  template<class Source>
  request_line get_request_line(Source &in) {
    while (expect(in, '\r'))
      if (!expect(in, '\n'))
        throw bad_format();
    request_line ret;
    get_until(' ', in, ret.get<REQUEST_METHOD>());
    if (ret.get<REQUEST_METHOD>().empty())
      throw bad_format();
    get_until(' ', in, ret.get<REQUEST_URI>());
    if (ret.get<REQUEST_URI>().empty())
      throw bad_format();
    get_until('\r', in, ret.get<REQUEST_HTTP_VERSION>());
    if (ret.get<REQUEST_HTTP_VERSION>().empty())
      throw bad_format();
    if(!expect(in, '\n'))
      throw bad_format();
    return ret;
  }

  typedef std::map<std::string, std::string> header_fields;

  // reads a header field from `in' and adds it to `fields'
  // see RFC 2616 chapter 4.2
  // Warning: Field names are converted to all lower-case!
  template<class Source, class HeaderFields>
  void get_header_field(Source &in, HeaderFields &fields) {
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

    std::string &value = fields[name];
    if (!value.empty())
      value += ", ";

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

    std::string::reverse_iterator xend = value.rbegin();
    while (isspht(*xend))
      ++xend;
    value.erase(xend.base(), value.end());
  }

  template<class Source, class HeaderFields>
  void read_headers(Source &source, HeaderFields &fields) {
    do {
      get_header_field(source, fields);
    } while (!(expect(source, '\r') && expect(source, '\n')));
  }

  void parse_parametrised(
      std::string const &in,
      std::string &element,
      std::set<std::string> const &interesting_parameters,
      std::map<std::string, std::string> &parameters);

  void parse_list(std::string const &in, std::vector<std::string> &out,
                  char delimeter = ',');

  void parse_qlist(std::string const &in, std::multimap<int, std::string> &out);

  int parse_qvalue(std::string const &in);
}}}

namespace rest {
  class cookie;
namespace utils { namespace http {
  void parse_cookie_header(std::string const &in,
                           std::vector<rest::cookie> &cookies);
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
}}

#endif
