// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SOCKET_PARAM_HPP
#define REST_SOCKET_PARAM_HPP

#include "network.hpp"
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

namespace rest {

class host_container;

class socket_param {
public:
  socket_param(
      std::string const &service,
      network::socket_type_t type,
      std::string const &bind,
      long timeout_read,
      long timeout_write);

  socket_param &operator=(socket_param o) {
    o.swap(*this);
    return *this;
  }

  void swap(socket_param &o) {
    p.swap(o.p);
  }

  std::string const &service() const;
  network::socket_type_t socket_type() const;
  std::string const &bind() const;

  long timeout_read() const;
  long timeout_write() const;

  host_container &hosts();

  host_container const &hosts() const {
    return const_cast<socket_param *>(this)->hosts();
  }

public: // internal
  int fd() const;
  void fd(int f);
    
private:
  class impl;
  boost::shared_ptr<impl> p;
};

typedef std::vector<socket_param> sockets_container;

}

#endif
