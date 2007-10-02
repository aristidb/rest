// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <errno.h>

#ifdef APPLE
#define TCP_CORK TCP_NOPUSH
#endif

using namespace rest::utils;

class socket_device::impl {
public:
  impl(int fd) : fd(fd), log_cork(0), tcp_cork(0) {}
  ~impl() { if (fd >= 0) close(); }

  void close() {
    if (fd >= 0)
      ::close(fd);
    fd = -1;
  }

  int fd;

  bool log_cork;
  bool tcp_cork;

  void do_cork(bool x) {
    #ifndef APPLE
    int const cork = x;
    ::setsockopt(fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));
    #endif
  }
};

socket_device::socket_device(int fd, long timeout_rd, long timeout_wr)
: p(new impl(fd))
{
  struct timeval timeout;
  timeout.tv_usec = 0;

  if (timeout_rd > 0) {
    timeout.tv_sec = timeout_rd;
    ::setsockopt(p->fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  }

  if (timeout_wr > 0) {
    timeout.tv_sec = timeout_wr;
    ::setsockopt(p->fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  }
}

socket_device::~socket_device() {
}

void socket_device::push_cork() {
  p->log_cork = true;
}

void socket_device::loosen_cork() {
  p->log_cork = false;
}

void socket_device::pull_cork() {
  p->log_cork = false;
  if (p->tcp_cork)
    p->do_cork(false);
}

void socket_device::close(std::ios_base::open_mode) {
  p->close();
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
  return n;
}

std::streamsize socket_device::write(char const *buf, std::streamsize length) {
  if (p->fd < 0)
    return -1;

  if (p->log_cork && !p->tcp_cork) {
    p->tcp_cork = true;
    p->do_cork(true);
  }

  std::streamsize n;
  for (;;) {
    n = ::write(p->fd, buf, size_t(length));
    if (n == length)
      break;
    if (n >= 0) {
      buf += n;
      length -= n;
    } else if (errno != EINTR)
      break;
  }
  if (n <= 0) {
    p->close();
    return -1;
  }
  return n;
}
