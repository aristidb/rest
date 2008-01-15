// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_RESPONDER_HPP
#define REST_RESPONDER_HPP

#include "response.hpp"
#include "cache.hpp"
#include <string>
#include <boost/any.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/or.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/type_traits/is_same.hpp>

namespace rest {

class keywords;
class request;

enum response_type {
  GET = 1U,
  PUT = 2U,
  POST = 4U,
  DELETE = 8U,
  ALL = GET | PUT | POST | DELETE
};

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
    virtual response get()=0;
    virtual ~get_base() {}
  };
  struct put_base {
    virtual response put()=0;
    virtual ~put_base() {}
  };
  struct post_base {
    virtual response post()=0;
    virtual ~post_base() {}
  };
  struct delete__base {
    virtual response delete_()=0;
    virtual ~delete__base() {}
  };

  struct responder_base {
    virtual get_base *x_getter() = 0;
    virtual put_base *x_putter() = 0;
    virtual post_base *x_poster() = 0;
    virtual delete__base *x_deleter() = 0;

    virtual void x_set_path(any_path const &) = 0;
    virtual void set_keywords(keywords &) = 0;
    virtual void set_request(request const &) = 0;
    virtual void set_time(time_t now) = 0;

    virtual void prepare() = 0;
    virtual void reset() = 0;

    virtual bool exists() const = 0;
    virtual std::string etag() const = 0;
    virtual time_t last_modified() const = 0;
    virtual time_t expires() const = 0;

    virtual cache::flags cache() const = 0;
    virtual cache::flags cache(std::string const &header) const = 0;

    virtual ~responder_base() {}
  };

  #define REST_METHOD_DEFINITION(method) \
    template<typename, bool> struct i_ ## method {}; \
    template<typename Path> \
    struct i_ ## method<Path, true> : method ## _base { \
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
  virtual bool exists() const {
    return true;
  }

  virtual time_t last_modified() const {
    return time_t(-1);
  }

  virtual std::string etag() const {
    return std::string();
  }

  virtual time_t expires() const
  {
    return time_t(-1);
  }

  virtual cache::flags cache() const
  {
    return cache::NO_FLAGS;
  }

  virtual cache::flags cache(std::string const &header) const
  {
    return cache::default_header_flags(header);
  }

public:
  virtual void prepare() {
  }

  virtual void reset() {
  }

  path_parameter get_path() const {
    return path;
  }

  keywords &get_keywords() const {
    return *p_keywords;
  }

  request const &get_request() const {
    return *p_request;
  }

  time_t get_time() const {
    return now;
  }

private:
  void x_set_path(detail::any_path const &x_path) {
    path = detail::unpack<path_type>(x_path);
  }

  void set_keywords(keywords &kw) {
    p_keywords = &kw;
  }

  void set_request(request const &req) {
    p_request = &req;
  }

  void set_time(time_t now_) {
    now = now_;
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

private:
  path_type path;
  keywords *p_keywords;
  request const *p_request;
  time_t now;
};

}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
