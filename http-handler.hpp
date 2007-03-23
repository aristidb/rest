// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_HTTP_HANDLER_HPP
#define REST_HTTP_HTTP_HANDLER_HPP

#include "rest.hpp"
#include <iosfwd>

namespace rest {
namespace http {
  class http_handler {
    std::streambuf &conn;
    bool head_method;
    bool http_1_0_compat;
  public:
    http_handler(std::streambuf &conn)
      : conn(conn), head_method(false), http_1_0_compat(false)
    { }

    response handle_request(context &global);
    void send(response const &r);
  };
}}

#endif
