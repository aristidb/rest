// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_CONTEXT_HPP
#define REST_CONTEXT_HPP

#include "responder.hpp"
#include "keywords.hpp"
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest {

class server;

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

  void attach(server &);

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
