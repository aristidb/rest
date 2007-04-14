// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HPP
#define REST_HPP

#include <string>
#include <vector>
#include <iosfwd>
#include <boost/mpl/void.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/any.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

namespace rest {

class response {
private: // TODO: make that less inline
  int code;
  std::string type;
  std::string data;
  std::string reason;

  static char const *default_reason(int code);

public:
  response(int code,
           std::string const &type="",
           std::string const &data="",
           std::string const &reason="")
    : code(code), type(type), data(data),
      reason(reason.empty() ? default_reason(code) : reason)
  {}
  response(std::string const &type)
  : code(200), type(type), reason(default_reason(code)) {}

  void set_data(std::string const &d) { data = d; }
  std::string const &get_data() const { return data; }
  void set_code(int c) { code = c; }
  int get_code() const { return code; }
  void set_reason(std::string const &r) { reason = r; }
  std::string const &get_reason() const { return reason; }
  std::string const &get_type() const { return type; }
  void set_type(std::string const &t) { type = t; }
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
  NONE = -1,
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

  void set_stream(std::string const &key, std::istream *value) {
    return set_stream(key, 0, value);
  }

  void set_stream(std::string const &key, int index, std::istream *stream);

  void set_name(std::string const &key, std::string const &value) {
    set_name(key, 0, value);
  }

  void set_name(std::string const &key, int index, std::string const &name);

  void set_entity(std::istream *entity, std::string const &type);

  void set_output(std::string const &key, std::ostream *stream) {
    set_output(key, 0, stream);
  }

  void set_output(std::string const &key, int index, std::ostream *output);

  void flush();

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
      return get(unpack<path_type>(path), kw);
    }
  };
  template<typename Path>
  struct putter<Path, true> : putter_base {
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response put(path_parameter, keywords &) = 0;
  private:
    response x_put(any_path const &path, keywords &kw) {
      return put(unpack<path_type>(path), kw);
    }
  };
  template<typename Path>
  struct poster<Path, true> : poster_base{
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response post(path_parameter, keywords &) = 0;
  private:
    response x_post(any_path const &path, keywords &kw) {
      return post(unpack<path_type>(path), kw);
    }
  };
  template<typename Path>
  struct deleter<Path, true> : deleter_base {
    typedef typename path_helper<Path>::path_parameter path_parameter;
    typedef typename path_helper<Path>::path_type path_type;
    virtual response delete_(path_parameter, keywords &) = 0;
  private:
    response x_delete(any_path const &path, keywords &kw) {
      return delete_(unpack<path_type>(path), kw);
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

class host : boost::noncopyable {
public:
  host(std::string const &);
  ~host();

  void set_host(std::string const &);
  std::string get_host() const;

  context &get_context() const;

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

class server : boost::noncopyable {
public:
  server();
  ~server();

  void serve();

  class socket_param {
  public:
    enum socket_type_t { ip4, ip6 };

    socket_param(short port, socket_type_t type);
    //socket_param(socket_param const &);
    ~socket_param();

    socket_param &operator=(socket_param o) {
      o.swap(*this);
      return *this;
    }

    void swap(socket_param &o) {
      p.swap(o.p);
    }

    short port() const;
    socket_type_t socket_type() const;

    void add_host(host const &);
    host const *get_host(std::string const &name);

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
  void set_config_socket(char const *file);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif

