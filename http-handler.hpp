// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_HTTP_HANDLER_HPP
#define REST_HTTP_HTTP_HANDLER_HPP

#include "rest.hpp"
#include "boost/asio.hpp"

namespace rest {
namespace http {
  typedef ::boost::asio::ip::tcp::iostream iostream;

  class http_handler {
    iostream &conn;
  public:
    http_handler(iostream &conn)
      : conn(conn)
    { }

    response handle_request(context &global);
    void send(response &r);
  };
}}

#endif
