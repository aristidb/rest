// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_HTTP_HANDLER_HPP
#define REST_HTTP_HTTP_HANDLER_HPP

#include "rest.hpp"
#include <iosfwd>
#include <bitset>

namespace rest {
namespace http {
  class http_handler {
    std::streambuf &conn;
    // should rather use std::vector<bool>, std::bitset or flags!
    typedef std::bitset<4> state_flags;
    enum { NO_ENTITY, HTTP_1_0_COMPAT, ACCEPT_GZIP, ACCEPT_BZIP2 };
    state_flags flags;

    bool head_method;
    bool http_1_0_compat;
    bool accept_gzip;
    bool accept_bzip2;
  public:
    http_handler(std::streambuf &conn)
      : conn(conn), head_method(false), http_1_0_compat(false),
        accept_gzip(false), accept_bzip2(false)
    { }

    response handle_request(context &global);
    void send(response const &r);
  };
}}

#endif
