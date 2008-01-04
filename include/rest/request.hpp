// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_REQUEST_HPP
#define REST_REQUEST_HPP

#include "network.hpp"
#include <string>
#include <boost/scoped_ptr.hpp>

namespace rest {

class host;
class headers;

class request {
public:
  request(network::address const &addr);
  ~request();

public:
  void clear();

public:
  void set_uri(std::string const &uri);
  std::string const &get_uri() const;

  void set_host(host const &h);
  host const &get_host() const;

  network::address const &get_client_address() const;

public:
  headers &get_headers();

  headers const &get_headers() const {
    return const_cast<request *>(this)->get_headers();
  }

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
