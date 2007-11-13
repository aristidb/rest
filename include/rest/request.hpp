// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_REQUEST_HPP
#define REST_REQUEST_HPP

#include "network.hpp"
#include <string>
#include <iosfwd>
#include <boost/optional.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest {

class host;

class request {
public:
  request(network::address const &addr);
  ~request();

  void set_uri(std::string const &uri);
  std::string const &get_uri() const;

  void set_host(host const &h);
  host const &get_host() const;

  void clear();

  void read_headers(std::streambuf&);

  boost::optional<std::string> get_header(std::string const &name) const;
  void erase_header(std::string const &name);

  typedef
    boost::function<void (std::string const &name, std::string const &value)>
    header_callback;

  void for_each_header(header_callback const &) const;

  network::address const &get_client_address() const;

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
