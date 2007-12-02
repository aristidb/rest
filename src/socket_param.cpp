// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/socket_param.hpp>
#include <rest/host.hpp>
#include <string>

using rest::socket_param;

class socket_param::impl {
public:
  impl(
      std::string const &service,
      network::socket_type_t type,
      std::string const &bind,
      long timeout_read,
      long timeout_write)
  : service(service),
    socket_type(type),
    bind(bind),
    timeout_read(timeout_read),
    timeout_write(timeout_write),
    fd(-1)
  { }

  std::string service;
  network::socket_type_t socket_type;
  std::string bind;
  long timeout_read;
  long timeout_write;

  host_container hosts;
  
  int fd;
};

socket_param::socket_param(
    std::string const &service,
    network::socket_type_t type,
    std::string const &bind,
    long timeout_read,
    long timeout_write
  )
: p(new impl(service, type, bind, timeout_read, timeout_write))
{ }

int socket_param::fd() const {
  return p->fd;
}

void socket_param::fd(int f) {
  p->fd = f;
} 

std::string const &socket_param::service() const {
  return p->service;
}

std::string const &socket_param::bind() const {
  return p->bind;
}

rest::network::socket_type_t socket_param::socket_type() const {
  return p->socket_type;
}

long socket_param::timeout_read() const {
  return p->timeout_read;
}

long socket_param::timeout_write() const {
  return p->timeout_write;
}

rest::host_container &socket_param::hosts() {
  return p->hosts;
}
