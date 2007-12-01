// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_CONNECTION_HPP
#define REST_HTTP_CONNECTION_HPP

#include "socket_param.hpp"
#include "request.hpp"
#include "response.hpp"
#include "keywords.hpp"
#include "responder.hpp"
#include "utils/socket_device.hpp"
#include <boost/iostreams/stream_buffer.hpp>
#include <bitset>
#include <string>

namespace rest {

typedef 
  boost::iostreams::stream_buffer<utils::socket_device>
  connection_streambuf;

class http_connection {
  socket_param const &sock;
  int connfd;
  connection_streambuf conn;

  std::string const &servername;

  bool open_;
  enum {
    NO_ENTITY,
    HTTP_1_0_COMPAT,
    X_NO_FLAG
  };
  typedef std::bitset<X_NO_FLAG> state_flags;
  state_flags flags;
  std::vector<response::content_encoding_t> encodings;

  request request_;

  typedef std::vector<std::pair<boost::int64_t, boost::int64_t> > ranges_t;
  ranges_t ranges;

public:
  http_connection(socket_param const &sock, int connfd,
                  rest::network::address const &addr,
                  std::string const &servername)
  : sock(sock),
    connfd(connfd),
    conn(connfd, sock.timeout_read(), sock.timeout_write()),
    servername(servername),
    open_(true),
    request_(addr)
  { }

  bool open() const { return open_; }

  int set_header_options();

  void reset() {
    flags.reset();
    encodings.clear();
  }

  response handle_request();
  int handle_entity(keywords &kw);

  void send(response r, bool entity);
  void send(response r) {
    send(r, !flags.test(NO_ENTITY));
  }

  void serve();

  int handle_modification_tags(
    time_t, std::string const &, std::string const &);

  void handle_caching(
    detail::responder_base *,
    detail::any_path const &,
    response &,
    time_t,
    time_t);

  void handle_header_caching(
    detail::responder_base *,
    detail::any_path const &,
    response &,
    bool &,
    std::string const &);

  void analyze_ranges();

  void tell_allow(response &resp, detail::responder_base *responder);

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
};

}

#endif
