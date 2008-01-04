// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_CONNECTION_HPP
#define REST_HTTP_CONNECTION_HPP

#include "socket_param.hpp"
#include "network.hpp"
#include "response.hpp"
#include "responder.hpp"
#include <iosfwd>
#include <string>
#include <boost/scoped_ptr.hpp>

namespace rest {

class keywords;

class http_connection {
public:
  http_connection(host_container const &hosts,
                  rest::network::address const &addr,
                  std::string const &servername);

  ~http_connection();

  void serve(socket_param const &sock, int connfd);
  void serve(std::istream &in, std::ostream &out);

private:
  void serve();

  int set_header_options();

  void reset();

  response handle_request();
  int handle_entity(keywords &kw);

  void send(response r, bool entity);
  void send(response r);

  int handle_modification_tags(
    time_t, std::string const &, std::string const &);

  void handle_caching(
    detail::responder_base *,
    detail::any_path const &,
    response &,
    time_t,
    time_t,
    keywords &,
    request const &);

  void handle_header_caching(
    detail::responder_base *,
    detail::any_path const &,
    response &,
    bool &,
    keywords &,
    request const &,
    std::string const &);

  void analyze_ranges();

  void tell_allow(response &resp, detail::responder_base *responder);

  static bool never_cache(int method);

public: //internal
  response handle_get(
    detail::responder_base *,
    detail::any_path const &,
    keywords &,
    request const &);

  response handle_head(
    detail::responder_base *,
    detail::any_path const &,
    keywords &,
    request const &);

  response handle_post(
    detail::responder_base *,
    detail::any_path const &,
    keywords &,
    request const &);

  response handle_put(
    detail::responder_base*,
    detail::any_path const &,
    keywords &,
    request const &);

  response handle_delete(
    detail::responder_base *,
    detail::any_path const &,
    keywords &,
    request const &);

  response handle_options(
    detail::responder_base *,
    detail::any_path const &,
    keywords &,
    request const &);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
