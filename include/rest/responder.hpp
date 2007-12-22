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

    virtual std::string x_etag(
      any_path const &, keywords &, request const &) const = 0;
    virtual time_t x_last_modified(
      any_path const &, time_t, keywords &, request const &) const = 0;
    virtual time_t x_expires(
      any_path const &, time_t, keywords &, request const &) const = 0;

    virtual cache::flags x_cache(
      any_path const &, keywords &, request const &) const = 0;
    virtual cache::flags x_cache(
      any_path const &, std::string const &, keywords &, request const &)
      const = 0;

    virtual ~responder_base() { }
  };

  #define REST_METHOD_DEFINITION(method)                                      \
    template<typename, bool> struct i_ ## method { };                         \
    template<typename Path>                                                   \
    struct i_ ## method<Path, true> : method ## _base {                       \
      typedef typename path_helper<Path>::path_parameter path_parameter;      \
      typedef typename path_helper<Path>::path_type path_type;                \
      virtual response method(path_parameter, keywords &, request const &)=0; \
    private:                                                                  \
      response x_ ## method(                                                  \
          any_path const &path, keywords &kw, request const &req)             \
      {                                                                       \
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

  virtual time_t last_modified(
    path_parameter, time_t, keywords &, request const &) const
  {
    return time_t(-1);
  }

  virtual std::string etag(path_parameter, keywords &, request const &) const {
    return std::string();
  }

  virtual time_t expires(
    path_parameter, time_t, keywords &, request const &) const
  {
    return time_t(-1);
  }

  virtual cache::flags cache(path_parameter, keywords &, request const&) const
  {
    return cache::NO_FLAGS;
  }

  virtual cache::flags cache(
    path_parameter, std::string const &header, keywords&, request const&) const
  {
    return cache::default_header_flags(header);
  }

private:
  bool x_exists(detail::any_path const &path, keywords &kw) const {
    return exists(detail::unpack<path_type>(path), kw);
  }

  time_t x_last_modified(
    detail::any_path const &path, time_t now, keywords &kw, request const &r)
    const
  {
    return last_modified(detail::unpack<path_type>(path), now, kw, r);
  }

  std::string x_etag(
    detail::any_path const &path, keywords &kw, request const &r) const
  {
    return etag(detail::unpack<path_type>(path), kw, r);
  }

  time_t x_expires(
    detail::any_path const &path, time_t now, keywords &kw, request const &r)
    const
  {
    return expires(detail::unpack<path_type>(path), now, kw, r);
  }

  cache::flags x_cache(
    detail::any_path const &path, keywords &kw, request const &r) const
  {
    return cache(detail::unpack<path_type>(path), kw, r);
  }

  cache::flags x_cache(
    detail::any_path const &path, std::string const &header, keywords &kw,
    request const &r) const
  {
    return cache(detail::unpack<path_type>(path), header, kw, r);
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
  virtual bool exists(keywords&) const {
    return true;
  }

  virtual time_t last_modified(time_t, keywords&, request const&) const {
    return time_t(-1);
  }

  virtual std::string etag(keywords&, request const&) const {
    return std::string();
  }

  virtual time_t expires(time_t, keywords&, request const&) const {
    return time_t(-1);
  }

  virtual cache::flags cache(keywords&, request const&) const {
    return cache::NO_FLAGS;
  }

  virtual cache::flags cache(
    std::string const &header, keywords&, request const&) const
  {
    return cache::default_header_flags(header);
  }

private:
  bool x_exists(detail::any_path const &, keywords &kw) const {
    return exists(kw);
  }

  time_t x_last_modified(
    detail::any_path const &, time_t now, keywords &kw, request const &r)
    const
  {
    return last_modified(now, kw, r);
  }

  std::string x_etag(
    detail::any_path const &, keywords &kw, request const &r) const
  {
    return etag(kw, r);
  }

  time_t x_expires(
    detail::any_path const &, time_t now, keywords &kw, request const &r)
    const
  {
    return expires(now, kw, r);
  }

  cache::flags x_cache(
    detail::any_path const &, keywords &kw, request const &r) const
  {
    return cache(kw, r);
  }

  cache::flags x_cache(
    detail::any_path const &, std::string const &header, keywords &kw,
    request const &r) const
  {
    return cache(header, kw, r);
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

}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
