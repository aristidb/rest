// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/network.hpp>
#include <rest/utils/exceptions.hpp>
#include <cstddef>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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

