// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/socket_param.hpp>
#include <rest/host.hpp>
#include <string>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <stdexcept>

using rest::socket_param;
using rest::host;
using namespace boost::multi_index;

typedef
  boost::multi_index_container<
    boost::reference_wrapper<host const>,
    indexed_by<
      hashed_unique<
        const_mem_fun<host, std::string, &host::get_host>
      >
    >
  >
  hosts_cont_t;

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

  hosts_cont_t hosts;
  
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

void socket_param::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

host const *socket_param::get_host(std::string const &name) const {
  std::string::const_iterator begin = name.begin();
  std::string::const_iterator end = name.end();
  std::string::const_iterator delim = std::find(begin, end, ':');

  std::string the_host(begin, delim);

  hosts_cont_t::const_iterator it = p->hosts.find(the_host);
  while(it == p->hosts.end() &&
        !the_host.empty())
  {
    std::string::const_iterator begin = the_host.begin();
    std::string::const_iterator end = the_host.end();
    std::string::const_iterator delim = std::find(begin, end, '.');

    if (delim == end)
      the_host.clear();
    else
      the_host.assign(++delim, end);

    it = p->hosts.find(the_host);
  }
  if(it == p->hosts.end())
    return 0x0;
  return it->get_pointer();
}

