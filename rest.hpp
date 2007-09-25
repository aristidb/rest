// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HPP
#define REST_HPP

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

class input_stream {
public:
  input_stream() : stream(0), own(false) {}

  explicit input_stream(std::istream &stream)
    : stream(&stream), own(false) {}

  explicit input_stream(std::istream *stream, bool own = true)
    : stream(stream), own(own)
  {}

  input_stream(input_stream &o)
    : stream(o.stream), own(o.own)
  { o.stream = 0; }

  void move(input_stream &x) {
    x.reset();
    x.stream = stream;
    x.own = own;
    stream = 0;
  }

  void reset();

  operator input_stream &() { return *this; }
  std::istream &operator*() const { return *stream; }
  std::istream *operator->() const { return stream; }
  std::istream *get() const { return stream; }
  ~input_stream() { reset(); }

private:
  std::istream *stream;
  bool own;

  input_stream &operator=(input_stream const &); //DUMMY
};

class output_stream {
public:
  output_stream() : stream(0), own(false) {}

  explicit
  output_stream(std::ostream &stream)
    : stream(&stream), own(false)
  {}

  explicit
  output_stream(std::ostream *stream, bool own = true)
    : stream(stream), own(own)
  {}

  output_stream(output_stream &o)
    : stream(o.stream), own(o.own)
  { o.stream = 0; }

  void move(output_stream &x) {
    x.reset();
    x.stream = stream;
    x.own = own;
    stream = 0;
  }

  operator output_stream &() { return *this; }
  std::ostream &operator*() const { return *stream; }
  std::ostream *operator->() const { return stream; }
  std::ostream *get() const { return stream; }
  ~output_stream() { reset(); }

  void reset();

private:
  std::ostream *stream;
  bool own;

  output_stream &operator=(output_stream const &); //DUMMY
};

struct cookie {
  std::string name;
  std::string value;

  std::string domain;
  time_t expires;
  std::string path;
  bool secure;

  cookie(std::string const &name, std::string const &value,
         time_t expires = time_t(-1), std::string const &path = "",
         std::string const &domain = "")
    : name(name), value(value), domain(domain),
      expires(expires), path(path), secure(false)
  { }
};

/*
 * TODO:
 * - multi-chunk output
 * - mmap output / sendfile
 */
class response {
public:
  response();
  response(int code);
  response(std::string const &type);
  response(std::string const &type, std::string const &data);
  response(response &r);
  ~response();

  operator response &() { return *this; }

  struct empty_tag_t {};
  response(empty_tag_t);
  static empty_tag_t empty_tag() { return empty_tag_t(); }

  void move(response &o);
  void swap(response &o);

  void set_code(int code);
  void set_type(std::string const &type);
  void set_header(std::string const &name, std::string const &value);
  void add_header_part(std::string const &name, std::string const &value);

  void add_cookie(cookie const &c);

  enum content_encoding_t {
    identity,
    deflate,
    gzip,
    bzip2,
    X_NO_OF_ENCODINGS
  };

  void set_data(std::string const &data,
    content_encoding_t content_encoding = identity);
  void set_data(input_stream &data, bool seekable,
    content_encoding_t content_encoding = identity);

  int get_code() const;
  static char const *reason(int code);
  std::string const &get_type() const;

  bool has_content_encoding(content_encoding_t content_encoding) const;

  content_encoding_t choose_content_encoding(
    std::vector<content_encoding_t> const &encodings) const;

  bool empty(content_encoding_t content_encoding) const;
  bool chunked(content_encoding_t content_encoding) const;
  std::size_t length(content_encoding_t content_encoding) const;

  void print_headers(std::ostream &out) const;
  void print_entity(
    std::ostream &out, content_encoding_t enc, bool may_chunk) const;

private:
  void print_cookie_header(std::ostream &out) const;
  void encode(std::ostream &out, content_encoding_t enc, bool may_chunk) const;
  void decode(std::ostream &out, content_encoding_t enc, bool may_chunk) const;

  class impl;
  boost::scoped_ptr<impl> p;

  response &operator=(response const &); //DUMMY
};

class request;

enum keyword_type {
  NORMAL,
  COOKIE,
  FORM_PARAMETER,
  HEADER,
  NONE = -1
};

class request;

class keywords {
public:
  keywords();
  ~keywords();

  bool exists(std::string const &key, int index = 0) const;

  std::string const &get(std::string const &keyword, int index = 0) const {
    return const_cast<keywords *>(this)->access(keyword, index);
  }

  std::string &access(std::string const &key, int index = 0);
  std::istream &read(std::string const &key, int index = 0);

  std::string get_name(std::string const &key, int index = 0) const;

  void declare(std::string const &key, int index, keyword_type type);
  keyword_type get_declared_type(std::string const &key, int index = 0) const;

  bool is_set(std::string const &key, int index = 0) const;

  void set(std::string const &key, int index, std::string const &value);
  void set_with_type(
    keyword_type, std::string const &key, int index, std::string const &value);
  void set_stream(std::string const &key, int index, input_stream &stream);
  void set_name(std::string const &key, int index, std::string const &name);
  void set_output(std::string const &key, int index, output_stream &output);

  void unset(std::string const &key, int index = 0);

  void flush();

  void set_entity(std::auto_ptr<std::istream> &entity, std::string const &type);
  void add_uri_encoded(std::string const &data);

  void set_request_data(request const &req);

public:
  std::string &operator[](std::string const &key) {
    return access(key);
  }

  void declare(std::string const &key, keyword_type type) {
    return declare(key, 0, type);
  }

  void set(std::string const &key, std::string const &value) {
    return set(key, 0, value);
  }

  void set_with_type(
      keyword_type type, std::string const &key, std::string const &value)
  {
    return set_with_type(type, key, 0, value);
  }

  void set_stream(std::string const &key, input_stream &value) {
    return set_stream(key, 0, value);
  }
  
  void set_name(std::string const &key, std::string const &value) {
    return set_name(key, 0, value);
  }

  void set_output(std::string const &key, output_stream &stream) {
    return set_output(key, 0, stream);
  }

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

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
class responder<ResponseType, void>
: public
  detail::responder_base,
  detail::i_get<void, ResponseType & GET>,
  detail::i_put<void, ResponseType & PUT>,
  detail::i_post<void, ResponseType & POST>,
  detail::i_delete_<void, ResponseType & DELETE>
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

