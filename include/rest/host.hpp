// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HOST_HPP
#define REST_HOST_HPP

#include <string>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest {

class context;
class response;

class host : boost::noncopyable {
public:
  host(std::string const &);
  ~host();

  void set_host(std::string const &);
  std::string get_host() const;

  context &get_context() const;

  struct add {
    add(host const &h) : h(h) {}

    template<typename T>
    void operator() (T &s) const {
      s.hosts().add_host(h);
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

class host_container {
public:
  host_container();
  ~host_container();

  void add_host(host const &);
  host const *get_host(std::string const &name) const;

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
