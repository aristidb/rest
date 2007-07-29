// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HPP
#define REST_HPP

#include <string>
#include <vector>
#include <iosfwd>
#include <sys/socket.h>
#include <boost/mpl/void.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/any.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

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

  enum content_encoding_t {
    identity,
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
    std::vector<content_encoding_t> const &encodings, bool may_chunk) const;

  bool empty(content_encoding_t content_encoding) const;
  bool chunked(content_encoding_t content_encoding) const;
  std::size_t length(content_encoding_t content_encoding) const;

  void print_headers(std::ostream &out) const;
  void print_entity(
    std::ostream &out, content_encoding_t enc, bool may_chunk) const;

private:
  void encode(std::ostream &out, content_encoding_t enc, bool may_chunk) const;
  void decode(std::ostream &out, content_encoding_t enc, bool may_chunk) const;

  class impl;
  boost::scoped_ptr<impl> p;

  response &operator=(response const &); //DUMMY
};

enum response_type {
  GET = 1U,
  PUT = 2U,
  POST = 4U,
  DELETE = 8U,
  ALL = GET | PUT | POST | DELETE
};

enum keyword_type {
  NORMAL,
  COOKIE,
  FORM_PARAMETER,
  NONE = -1
};

class keywords {
public:
  keywords();
  ~keywords();

  bool exists(std::string const &key, int index = 0) const;

  std::string const &get(std::string const &keyword, int index = 0) const {
    return const_cast<keywords *>(this)->access(keyword, index);
  }

  std::string &access(std::string const &key, int index = 0);

  std::string &operator[](std::string const &key) {
    return access(key);
  }

  std::istream &read(std::string const &key, int index = 0);

  std::string get_name(std::string const &key, int index = 0) const;

  void declare(std::string const &key, int index, keyword_type type);

  void declare(std::string const &key, keyword_type type) {
    declare(key, 0, type);
  }

  keyword_type get_declared_type(std::string const &key, int index = 0) const;

  void set(std::string const &key, std::string const &value) {
    return set(key, 0, value);
  }

  void set(std::string const &key, int index, std::string const &value);

  void set_stream(std::string const &key, input_stream &value) {
    return set_stream(key, 0, value);
  }

  void set_stream(
      std::string const &key, int index, input_stream &stream);

  void set_name(std::string const &key, std::string const &value) {
    set_name(key, 0, value);
  }

  void set_name(std::string const &key, int index, std::string const &name);

  void set_output(std::string const &key, output_stream &stream) {
    set_output(key, 0, stream);
  }

  void set_output(
      std::string const &key, int index, output_stream &output);

  void flush();

  void set_entity(std::auto_ptr<std::istream> &entity, std::string const &type);

  void add_uri_encoded(std::string const &data);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

namespace detail {
  typedef boost::any any_path;

  template<class T>
  T const &unpack(any_path const &p) {
    return *boost::unsafe_any_cast<T>(&p);
  }

  template<typename Path>
  struct path_helper {
    typedef typename
      boost::mpl::if_<
        boost::mpl::is_void_<Path>,
        std::string,
        Path
      >::type
      path_type;

    typedef typename
      boost::mpl::if_<
        boost::is_scalar<path_type>,
        path_type,
        path_type const &
      >::type
      path_parameter;
  };

  struct getter_base {
    virtual response x_get(any_path const &, keywords &) = 0;
    virtual ~getter_base() {}
  };
  struct putter_base {
    virtual response x_put(any_path const &, keywords &) = 0;
    virtual ~putter_base() {}
  };
  struct poster_base {
    virtual response x_post(any_path const &, keywords &) = 0;
    virtual ~poster_base() {}
  };
  struct deleter_base {
    virtual response x_delete(any_path const &, keywords &) = 0;
    virtual ~deleter_base() {}
  };

  struct responder_base {
    virtual getter_base *x_getter() = 0;
    virtual putter_base *x_putter() = 0;
    virtual poster_base *x_poster() = 0;
    virtual deleter_base *x_deleter() = 0;

    virtual bool x_exists(any_path const &, keywords &) const = 0;

    virtual std::string etag() const { return std::string(); }
    virtual time_t last_modified() const { return time_t(-1); }

    virtual ~responder_base() {}
  };

  template<typename, bool> struct getter {};
  template<typename, bool> struct putter {};
  template<typename, bool> struct poster {};
  template<typename, bool> struct deleter {};

  template<typename Path>
  struct getter<Path, true> : getter_base {
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response get(path_parameter, keywords &) = 0;
  private:
    response x_get(any_path const &path, keywords &kw) {
      response result(response::empty_tag());
      get(unpack<path_type>(path), kw).move(result);
      return result;
    }
  };
  template<typename Path>
  struct putter<Path, true> : putter_base {
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response put(path_parameter, keywords &) = 0;
  private:
    response x_put(any_path const &path, keywords &kw) {
      response result(response::empty_tag());
      put(unpack<path_type>(path), kw).move(result);
      return result;
    }
  };
  template<typename Path>
  struct poster<Path, true> : poster_base{
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response post(path_parameter, keywords &) = 0;
  private:
    response x_post(any_path const &path, keywords &kw) {
      response result(response::empty_tag());
      post(unpack<path_type>(path), kw).move(result);
      return result;
    }
  };
  template<typename Path>
  struct deleter<Path, true> : deleter_base {
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response delete_(path_parameter, keywords &) = 0;
  private:
    response x_delete(any_path const &path, keywords &kw) {
      response result(response::empty_tag());
      delete_(unpack<path_type>(path), kw).move(result);
      return result;
    }
  };
}

template<
  unsigned ResponseType = ALL,
  typename Path = boost::mpl::void_
>
class responder 
: public
  detail::responder_base,
  detail::getter<Path, ResponseType & GET>,
  detail::putter<Path, ResponseType & PUT>,
  detail::poster<Path, ResponseType & POST>,
  detail::deleter<Path, ResponseType & DELETE>
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

private:
  bool x_exists(detail::any_path const &path, keywords &kw) const {
    return exists(detail::unpack<path_type>(path), kw);
  }

private:
  detail::getter_base *x_getter() {
    return (ResponseType & GET) ? (detail::getter_base *) this : 0;
  }
  detail::putter_base *x_putter() {
    return (ResponseType & PUT) ? (detail::putter_base *) this : 0;
  }
  detail::poster_base *x_poster() {
    return (ResponseType & POST) ? (detail::poster_base *) this : 0;
  }
  detail::deleter_base *x_deleter() {
    return (ResponseType & DELETE) ? (detail::deleter_base *) this : 0;
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

class host;

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

    socket_param(short port, socket_type_t type, std::string const &bind = "");
    socket_param(std::string const &service, socket_type_t type,
                 std::string const &bind = "");
    //socket_param(socket_param const &);
    ~socket_param();

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

