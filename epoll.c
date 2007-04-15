#include "epoll.h"
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

int epoll_create(int size) {
  (void)size; // omit warning
  return kqueue();
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  int flags = 0;
  if(op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD)
    flags |= EV_ADD | EV_ENABLE;
  else if(op == EPOLL_CTL_DEL)
    flags |= EV_DELETE;
  else {
    errno = EINVAL;
    return -1;
  }

  int filter = 0;
  if(op != EPOLL_CTL_DEL) {
    assert(event);
    if(event->events & EPOLLIN || event->events & EPOLLERR ||
       event->events & EPOLLHUP)
      filter |= EVFILT_READ;
    if(event->events & EPOLLOUT)
      filter |= EVFILT_WRITE;
    if(event->events & EPOLLONESHOT)
      flags |= EV_ONESHOT;
    // TODO ...
  }

  struct kevent event_;
  EV_SET(&event_, fd, filter, flags, 0, 0, event);
  return kevent(epfd, &event_, 1, 0, 0, 0);
}

int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int milliseconds)
{
  struct timespec timeout;
  struct timespec const *timeout_ = 0x0;
  if(milliseconds != -1) {
    timeout.tv_sec = milliseconds / 1000; 
    timeout.tv_nsec = (milliseconds % 1000) * 1000000;
    timeout_ = &timeout;
  }

  struct kevent *kevents = malloc(sizeof(struct kevent) * maxevents);
  if(!kevents) {
    errno = EINVAL;
    return -1;
  }

  int n = kevent(epfd, 0, 0, kevents, maxevents, timeout_);
  if(n != -1) {
    int ev_pos = 0;
    for(int i = 0; i < n; ++i) {
      events[ev_pos].data = ((struct epoll_event*)kevents[i].udata)->data;
      events[ev_pos].events = 0;

      if(kevents[i].flags & EV_ERROR)
        events[ev_pos].events |= EPOLLERR;
      if(kevents[i].flags & EV_EOF)
        events[ev_pos].events |= EPOLLHUP;

      if(kevents[i].filter == EVFILT_READ)
        events[ev_pos].events |= EPOLLIN;
      if(kevents[i].filter == EVFILT_WRITE)
        events[ev_pos].events |= EPOLLOUT;

      ++ev_pos;
    }
  }

  free(kevents);
  return n;
}

