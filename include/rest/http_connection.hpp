// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_CONNECTION_HPP
#define REST_HTTP_CONNECTION_HPP

#include "socket_param.hpp"
#include "network.hpp"
#include "response.hpp"
#include "responder.hpp"
#include "logger.hpp"
#include <iosfwd>
#include <string>
#include <boost/scoped_ptr.hpp>

namespace rest {

class keywords;

class http_connection {
public:
  http_connection(host_container const &hosts,
                  rest::network::address const &addr,
                  std::string const &servername,
                  logger *log);

  ~http_connection();

  void serve(socket_param const &sock, int connfd);
  void serve(std::istream &in, std::ostream &out);

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
