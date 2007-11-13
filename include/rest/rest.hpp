// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HPP
#define REST_HPP

#include "keywords.hpp"
#include "response.hpp"
#include <map>
#include <string>
#include <vector>
#include <iosfwd>
#include <sys/socket.h>
#include <boost/cstdint.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/or.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/any.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/optional/optional.hpp>

namespace rest {

class input_stream;
class output_stream;
class cookie;

class request;

namespace network {
typedef union {
  boost::uint32_t ip4;
  boost::uint64_t ip6[2];
} addr_t;

struct address {
  int type;
  addr_t addr;
};

std::string ntoa(address const &addr);
}

class host;

class request {
public:
  request(network::address const &addr);
  ~request();

  void set_uri(std::string const &uri);
  std::string const &get_uri() const;

  void set_host(host const &h);
  host const &get_host() const;

  void clear();

  void read_headers(std::streambuf&);

  boost::optional<std::string> get_header(std::string const &name) const;
  void erase_header(std::string const &name);

  typedef
    boost::function<void (std::string const &name, std::string const &value)>
    header_callback;

  void for_each_header(header_callback const &) const;

  network::address const &get_client_address() const;
private:
  class impl;
  boost::scoped_ptr<impl> p;
};

enum response_type {
  GET = 1U,
  PUT = 2U,
  POST = 4U,
  DELETE = 8U,
  ALL = GET | PUT | POST | DELETE
};

struct NO_PATH {};
struct DEDUCED_PATH {};

namespace cache {
  enum flags {
    NO_FLAGS = 0U,
    private_ = 1U,
    no_cache = 2U,
    no_store = 4U,
    no_transform = 8U
  };

  inline flags operator~(flags x) {
    return flags(~(unsigned)x);
  }

  inline flags operator|(flags a, flags b) {
    return flags((unsigned)a | (unsigned)b);
  }

  inline flags &operator|=(flags &a, flags b) {
    return a = (a | b);
  }

  inline flags operator&(flags a, flags b) {
    return flags((unsigned)a & (unsigned)b);
  }

  inline flags &operator&=(flags &a, flags b) {
    return a = (a & b);
  }

  inline flags default_header_flags(std::string const &header) {
    if (header == "set-cookie" || header == "set-cookie2")
      return no_cache;
    return NO_FLAGS;
  }
}

namespace detail {
  typedef boost::any any_path;

  template<class T>
  T const &unpack(any_path const &p) {
    return *boost::unsafe_any_cast<T>(&p);
  }

  template<typename Path>
  struct path_helper {
    typedef typename boost::mpl::if_<
        boost::is_same<Path, DEDUCED_PATH>,
        std::string,
        Path
      >::type path_type;

    typedef typename boost::mpl::if_<
        boost::is_scalar<path_type>,
        path_type,
        path_type const &
      >::type path_parameter;
  };

  struct get_base {
    virtual response x_get(any_path const &, keywords &, request const &)=0;
    virtual ~get_base() {}
  };
  struct put_base {
    virtual response x_put(any_path const &, keywords &, request const &)=0;
    virtual ~put_base() {}
  };
  struct post_base {
    virtual response x_post(any_path const &, keywords &, request const &)=0;
    virtual ~post_base() {}
  };
  struct delete__base {
    virtual response x_delete_(any_path const &, keywords &, request const &)=0;
    virtual ~delete__base() {}
  };

  struct responder_base {
    virtual get_base *x_getter() = 0;
    virtual put_base *x_putter() = 0;
    virtual post_base *x_poster() = 0;
    virtual delete__base *x_deleter() = 0;

    virtual bool x_exists(any_path const &, keywords &) const = 0;

    virtual std::string x_etag(any_path const &) const = 0;
    virtual time_t x_last_modified(any_path const &, time_t) const = 0;
    virtual time_t x_expires(any_path const &, time_t) const = 0;

    virtual cache::flags x_cache(any_path const &) const = 0;
    virtual cache::flags x_cache(any_path const &, std::string const &) const=0;

    virtual ~responder_base() {}
  };

  #define REST_METHOD_DEFINITION(method) \
    template<typename, bool> struct i_ ## method {}; \
    template<typename Path> \
    struct i_ ## method<Path, true> : method ## _base { \
      typedef typename path_helper<Path>::path_parameter path_parameter; \
      typedef typename path_helper<Path>::path_type path_type; \
      virtual response method(path_parameter, keywords &, request const &)=0; \
    private: \
      response x_ ## method( \
          any_path const &path, keywords &kw, request const &req) \
      { \
        response result(response::empty_tag()); \
        method(unpack<path_type>(path), kw, req).move(result); \
        return result; \
      } \
    }; \
    template<> \
    struct i_ ## method<NO_PATH, true> : method ## _base { \
      virtual response method(keywords &, request const &) = 0; \
    private: \
      response x_ ## method( \
          any_path const &, keywords &kw, request const &req) \
      { \
        response result(response::empty_tag()); \
        method(kw, req).move(result); \
        return result; \
      } \
    }; \
    /**/

  REST_METHOD_DEFINITION(get)
  REST_METHOD_DEFINITION(put)
  REST_METHOD_DEFINITION(post)
  REST_METHOD_DEFINITION(delete_)

  #undef REST_METHOD_DEFINITION
}

template<
  unsigned ResponseType = ALL,
  typename Path = DEDUCED_PATH
>
class responder 
: public
  detail::responder_base,
  detail::i_get<Path, ResponseType & GET>,
  detail::i_put<Path, ResponseType & PUT>,
  detail::i_post<Path, ResponseType & POST>,
  detail::i_delete_<Path, ResponseType & DELETE>
{
public:
  static unsigned const flags = ResponseType;

  typedef typename detail::path_helper<Path>::path_type path_type;
  typedef typename detail::path_helper<Path>::path_parameter path_parameter;

  responder &get_interface() { return *this; }

  static detail::any_path pack(Path const &p) {
    return p;
  }

protected:
  virtual bool exists(path_parameter, keywords &) const {
    return true;
  }

  virtual time_t last_modified(path_parameter, time_t) const {
    return time_t(-1);
  }

  virtual std::string etag(path_parameter) const {
    return std::string();
  }

  virtual time_t expires(path_parameter, time_t) const {
    return time_t(-1);
  }

  virtual cache::flags cache(path_parameter) const {
    return cache::NO_FLAGS;
  }

  virtual cache::flags cache(path_parameter, std::string const &header) const {
    return cache::default_header_flags(header);
  }

private:
  bool x_exists(detail::any_path const &path, keywords &kw) const {
    return exists(detail::unpack<path_type>(path), kw);
  }

  time_t x_last_modified(detail::any_path const &path, time_t now) const {
    return last_modified(detail::unpack<path_type>(path), now);
  }

  std::string x_etag(detail::any_path const &path) const {
    return etag(detail::unpack<path_type>(path));
  }

  time_t x_expires(detail::any_path const &path, time_t now) const {
    return expires(detail::unpack<path_type>(path), now);
  }

  cache::flags x_cache(detail::any_path const &path) const {
    return cache(detail::unpack<path_type>(path));
  }

  cache::flags x_cache(
      detail::any_path const &path, std::string const &header) const
  {
    return cache(detail::unpack<path_type>(path), header);
  }

private:
  detail::get_base *x_getter() {
    return (ResponseType & GET) ? (detail::get_base *) this : 0;
  }
  detail::put_base *x_putter() {
    return (ResponseType & PUT) ? (detail::put_base *) this : 0;
  }
  detail::post_base *x_poster() {
    return (ResponseType & POST) ? (detail::post_base *) this : 0;
  }
  detail::delete__base *x_deleter() {
    return (ResponseType & DELETE) ? (detail::delete__base *) this : 0;
  }
};

template<unsigned ResponseType>
class responder<ResponseType, NO_PATH>
: public
  detail::responder_base,
  detail::i_get<NO_PATH, ResponseType & GET>,
  detail::i_put<NO_PATH, ResponseType & PUT>,
  detail::i_post<NO_PATH, ResponseType & POST>,
  detail::i_delete_<NO_PATH, ResponseType & DELETE>
{
public:
  static unsigned const flags = ResponseType;

  responder &get_interface() { return *this; }

protected:
  virtual bool exists(keywords &) const {
    return true;
  }

  virtual time_t last_modified(time_t) const {
    return time_t(-1);
  }

  virtual std::string etag() const {
    return std::string();
  }

  virtual time_t expires(time_t) const {
    return time_t(-1);
  }

  virtual cache::flags cache() const {
    return cache::NO_FLAGS;
  }

  virtual cache::flags cache(std::string const &header) const {
    return cache::default_header_flags(header);
  }

private:
  bool x_exists(detail::any_path const &, keywords &kw) const {
    return exists(kw);
  }

  time_t x_last_modified(detail::any_path const &, time_t now) const {
    return last_modified(now);
  }

  std::string x_etag(detail::any_path const &) const {
    return etag();
  }

  time_t x_expires(detail::any_path const &, time_t now) const {
    return expires(now);
  }

  cache::flags x_cache(detail::any_path const &) const {
    return cache();
  }

  cache::flags x_cache(detail::any_path const&, std::string const&header)const{
    return cache(header);
  }

private:
  detail::get_base *x_getter() {
    return (ResponseType & GET) ? (detail::get_base *) this : 0;
  }
  detail::put_base *x_putter() {
    return (ResponseType & PUT) ? (detail::put_base *) this : 0;
  }
  detail::post_base *x_poster() {
    return (ResponseType & POST) ? (detail::post_base *) this : 0;
  }
  detail::delete__base *x_deleter() {
    return (ResponseType & DELETE) ? (detail::delete__base *) this : 0;
  }
};

class context : boost::noncopyable {
public:
  context();
  ~context();

  void declare_keyword(std::string const &name, keyword_type type);
  keyword_type get_keyword_type(std::string const &name) const;
  void enum_keywords(
    keyword_type,
    boost::function<void (std::string const &)> const &) const;
  void prepare_keywords(keywords &) const;

  template<class T>
  void bind(std::string const &a, T &r) {
    do_bind(a, r.get_interface(), detail::any_path());
  }
  template<class T, class U>
  void bind(std::string const &a, T &r, U const &x) {
    do_bind(a, r.get_interface(), r.pack(x));
  }

  context &get_interface() { return *this; }

  void find_responder(
    std::string const &,
    detail::any_path &,
    detail::responder_base *&,
    context *&,
    keywords &);

private:
  void do_bind(
    std::string const &, detail::responder_base &, detail::any_path const &);

  void do_bind(
    std::string const &, context &, detail::any_path const &);

  template<class Iterator>
  void do_find_responder(
    Iterator, Iterator,
    detail::any_path &,
    detail::responder_base *&,
    context *&,
    keywords &);

  class impl;
  boost::scoped_ptr<impl> p;
};

namespace utils {
  class property_tree;
}

class server : boost::noncopyable {
public:
  server(utils::property_tree const &config);
  ~server();

  void serve();

  class socket_param {
  public:
    enum socket_type_t { ip4 = AF_INET, ip6 = AF_INET6 };

    socket_param(
        std::string const &service,
        socket_type_t type,
        std::string const &bind,
        long timeout_read,
        long timeout_write);

    socket_param &operator=(socket_param o) {
      o.swap(*this);
      return *this;
    }

    void swap(socket_param &o) {
      p.swap(o.p);
    }

    std::string const &service() const;
    socket_type_t socket_type() const;
    std::string const &bind() const;

    long timeout_read() const;
    long timeout_write() const;

    void add_host(host const &);
    host const *get_host(std::string const &name) const;

  public: // internal
    int fd() const;
    void fd(int f);
    
  private:
    class impl;
    boost::shared_ptr<impl> p;
  };

  typedef std::vector<socket_param>::iterator sockets_iterator;
  typedef std::vector<socket_param>::const_iterator sockets_const_iterator;

  sockets_iterator add_socket(socket_param const &);

  sockets_iterator sockets_begin();
  sockets_iterator sockets_end();
  sockets_const_iterator sockets_begin() const;
  sockets_const_iterator sockets_end() const;

  void sockets_erase(sockets_iterator);
  void sockets_erase(sockets_iterator, sockets_iterator);

  void set_listen_q(int no);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

class host : boost::noncopyable {
public:
  host(std::string const &);
  ~host();

  void set_host(std::string const &);
  std::string get_host() const;

  context &get_context() const;

  struct add {
    add(host const &h) : h(h) {}

    void operator() (server::socket_param &s) const {
      s.add_host(h);
    }

    host const &h;
  };

  void make_standard_response(response &) const;
  void set_standard_response(
    int code, std::string const &mime, std::string const &text);

private:
  template<class T>
  static void delete_helper(void *p) { delete static_cast<T *>(p); }

public:
  template<class T>
  void store(T *p) {
    do_store(p, &delete_helper<T>);
  }

private:
  void do_store(void *, void (*)(void*));

  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif

