// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HPP
#define REST_HPP

#include <string>
#include <boost/mpl/void.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/any.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

namespace rest {

class response {
  int code;
public:
  response(int code) : code(code) {}
  response(std::string const &type) {(void)type;}
  void set_data(std::string const &data) {(void)data;}
  void set_cookie(std::string const &name, std::string const &value) {
    (void)name; (void)value;
  }

  int getcode() const { return code; } //TEST only
};

enum response_type {
  GET = 1U,
  PUT = 2U,
  POST = 4U,
  DELETE = 8U,
  ALL = GET | PUT | POST | DELETE
};

class keywords {
public:
  keywords();
  ~keywords();

  std::string &operator[](std::string const &);
  std::istream &read(std::string const &);
  std::string name(std::string const &);

  void set(std::string const &, std::string const &);
  void set_stream(std::string const &, std::string const &, std::istream &);

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

#if 0
  class context *get_context();
  void set_context(class context *);
#endif

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

enum keyword_type {
  COOKIE,
  FORM_PARAMETER,
  NONE = -1,
};

class context : boost::noncopyable {
public:
  context();
  ~context();

  void declare_keyword(std::string const &name, keyword_type type);
  keyword_type get_keyword_type(std::string const &name) const;
  void enum_keywords(
    keyword_type,
    boost::function<void (std::string const &)> const &);

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

}

#endif

