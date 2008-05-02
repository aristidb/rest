// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTP_CONNECTION_HPP
#define REST_HTTP_CONNECTION_HPP

#include "network.hpp"
#include "response.hpp"
#include "responder.hpp"
#include <iosfwd>
#include <string>
#include <memory>
#include <boost/scoped_ptr.hpp>

namespace rest {

class keywords;
class host_container;
class logger;

class http_connection {
public:
  http_connection(host_container const &hosts,
                  rest::network::address const &addr,
                  std::string const &servername,
                  logger *log);

  ~http_connection();

  void serve(std::auto_ptr<std::streambuf> conn);

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
