// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_NETWORK_HPP
#define REST_NETWORK_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <string>
#include <boost/cstdint.hpp>

struct addrinfo;

namespace rest {

class socket_param;

namespace network {

enum socket_type_t { ip4 = AF_INET, ip6 = AF_INET6 };

typedef union {
  boost::uint32_t ip4;
  boost::uint64_t ip6[2];
} addr_t;

struct address {
  socket_type_t type;
  addr_t addr;
};

std::string ntoa(address const &addr);

int socket(int type);
void close_on_exec(int fd);
void getaddrinfo(socket_param const &sock, ::addrinfo **res);
int accept(socket_param const &sock, address &remote);
int create_listenfd(socket_param &sock, int backlog);

}}

#endif
