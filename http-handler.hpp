// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_HTTP_HANDLER_HPP
#define REST_HTTP_HTTP_HANDLER_HPP

#include "rest.hpp"
#include <iosfwd>

namespace rest {
namespace http {
  class http_handler {
    std::iostream &conn;
    bool head_method;
  public:
    http_handler(std::iostream &conn)
      : conn(conn), head_method(false)
    { }

    response handle_request(context &global);
    void send(response const &r);
  };
}}

#endif
