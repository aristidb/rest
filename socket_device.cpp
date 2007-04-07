// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>//DEBUG

using namespace rest::utils;

class socket_device::impl {
public:
  impl(int fd) : fd(fd) {}
  ~impl() { if (fd >= 0) close(); }

  void close() {
    ::close(fd);
    fd = -1;
  }

  int fd;
};

socket_device::socket_device(int fd, long timeout_s)
: p(new impl(fd))
{
  struct timeval timeout;
  timeout.tv_sec = timeout_s;
  timeout.tv_usec = 0;
  ::setsockopt(p->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(p->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

socket_device::~socket_device() {
}

std::streamsize socket_device::read(char *buf, std::streamsize length) {
  if (p->fd < 0)
    return -1;
  std::streamsize n;
  do {
    n = ::read(p->fd, buf, size_t(length));
  } while (n < 0 && errno == EINTR);
  if (n <= 0) {
    p->close();
    return -1;
  }
  std::cout << ",read " << n << " (" << length << ") bytes:\n-{{";
  std::cout.write(buf, n);
  std::cout << "}}-\n" << std::flush;
  return n;
}

std::streamsize socket_device::write(char const *buf, std::streamsize length) {
  if (p->fd < 0)
    return -1;
  std::streamsize n;
  do {
    n = ::write(p->fd, buf, size_t(length));
  } while (n < 0 && errno == EINTR);
  if (n <= 0) {
    p->close();
    return -1;
  }
  std::cout << ",wrote " << n << " (" << length << ") bytes:\n+{{";
  std::cout.write(buf, n);
  std::cout << "}}+\n" << std::flush;
  return n;
}
