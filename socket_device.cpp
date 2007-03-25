// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace rest::utils;

socket_device::socket_device(int fd, long timeout_s)
: fd(fd)
{
  struct timeval timeout;
  timeout.tv_sec = timeout_s;
  timeout.tv_usec = 0;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

socket_device::~socket_device() {
  close(fd);
}

std::streamsize socket_device::read(char *buf, std::streamsize length) {
  std::streamsize n = ::read(fd, buf, size_t(length));
  return n ? n : -1;
}

std::streamsize socket_device::write(char const *buf, std::streamsize length) {
  std::streamsize n = ::write(fd, buf, size_t(length));
  return n ? n : -1;
}
