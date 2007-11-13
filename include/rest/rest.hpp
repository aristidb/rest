// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HPP
#define REST_HPP

#include "responder.hpp"
#include "cache.hpp"
#include "network.hpp"
#include "keywords.hpp"
#include "response.hpp"
#include <map>
#include <string>
#include <vector>
#include <iosfwd>
#include <sys/socket.h>
#include <boost/cstdint.hpp>
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

class host;

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

