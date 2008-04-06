// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/network.hpp>
#include <rest/socket_param.hpp>
#include <rest/utils/exceptions.hpp>
#include <cstddef>
#include <boost/static_assert.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

const std::size_t MAX_IP_LEN = 41;

std::string rest::network::ntoa(network::address const &a) {
  char buf[MAX_IP_LEN] = { 0 };
  if(!::inet_ntop(a.type, &a.addr, buf, MAX_IP_LEN - 1))
    throw utils::errno_error("inet_ntop");
  return buf;
}

int rest::network::socket(int type) {
  int sock = ::socket(type, SOCK_STREAM, 0);
  if (sock == -1)
    throw utils::errno_error("could not start server (socket)");
  close_on_exec(sock);
  return sock;
}

void rest::network::close_on_exec(int fd) {
  if(::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
    throw utils::errno_error("fcntl");
}

void rest::network::getaddrinfo(socket_param const &sock, addrinfo **res) {
  addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = sock.socket_type();
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  char const *hostname = sock.bind().empty() ? 0x0 : sock.bind().c_str();
  int n = ::getaddrinfo(hostname, sock.service().c_str(), &hints, res);
  if(n != 0)
    throw std::runtime_error(std::string("getaddrinfo failed: ") +
                             gai_strerror(n));
}

int rest::network::accept(socket_param const &sock, address &addr) {
  int connfd = -1;
  switch ((addr.type = sock.socket_type())) {
  case network::ip4: {
      sockaddr_in cliaddr;
      socklen_t clilen = sizeof(cliaddr);
      connfd = ::accept(sock.fd(), (sockaddr *) &cliaddr, &clilen);
      BOOST_STATIC_ASSERT((sizeof(addr.addr.ip4) == sizeof(cliaddr.sin_addr)));
      std::memcpy(&addr.addr.ip4, &cliaddr.sin_addr, sizeof(addr.addr.ip4));
    }
    break;
  case network::ip6: {
      sockaddr_in6 cliaddr;
      socklen_t clilen = sizeof(cliaddr);
      connfd = ::accept(sock.fd(), (sockaddr *) &cliaddr, &clilen);
      BOOST_STATIC_ASSERT((sizeof(addr.addr.ip6) == sizeof(cliaddr.sin6_addr)));
      std::memcpy(addr.addr.ip6, &cliaddr.sin6_addr, sizeof(addr.addr.ip6));
    }
    break;
  };
  return connfd;
}

int rest::network::create_listenfd(socket_param &sock, int backlog) {
  addrinfo *res;
  getaddrinfo(sock, &res);
  addrinfo *const ressave = res;

  int listenfd;
  do {
    listenfd = socket(sock.socket_type());

    int const one = 1;
    ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if(::bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
      break;

    ::close(listenfd);
  } while( (res = res->ai_next) != 0x0 );
  ::freeaddrinfo(ressave);

  if(res == 0x0)
    throw utils::errno_error("could not start server (listen)");

  if(::listen(listenfd, backlog) == -1)
    throw utils::errno_error("could not start server (listen)");

  sock.fd(listenfd);

  return listenfd;
}

