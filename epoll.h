#ifndef EPOLL_H
#define EPOLL_H

// epoll to kqueue wrapper

#include <stdint.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

enum EPOLL_EVENTS {
  EPOLLIN = 0x001,
  EPOLLPRI = 0x002,
  EPOLLOUT = 0x004,
  EPOLLRDNORM = 0x040,
  EPOLLRDBAND = 0x080,
  EPOLLWRNORM = 0x100,
  EPOLLWRBAND = 0x200,
  EPOLLMSG = 0x400,
  EPOLLERR = 0x008,
  EPOLLHUP = 0x010,
  EPOLLONESHOT = (1 << 30),
  EPOLLET = (1 << 31)
};

enum EPOLL_MODES {
  EPOLL_CTL_ADD = 1,
  EPOLL_CTL_DEL = 2,
  EPOLL_CTL_MOD = 3
};

typedef union epoll_data {
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} epoll_data_t;

struct epoll_event {
  uint32_t events;
  epoll_data_t data;
};

#ifdef __cplusplus
extern "C" {
#endif

extern int epoll_create(int size);
extern int epoll_ctl(int epfd, int op, int fd,
                     struct epoll_event *event);
extern int epoll_wait(int epfd, struct epoll_event *events,
                      int maxevents, int timeout); 

#ifdef __cplusplus
}
#endif

#endif
