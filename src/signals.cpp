// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/signals.hpp"
#include <vector>

using rest::signals;

static std::vector<signals*> handlers;

static void common_handler(int sig) {
  if (handlers.size() <= sig || handlers[sig] == 0)
    _exit(6);
  handlers[sig]->handle(sig);
}

class signals::impl {
public:
  sigset_t members;
  sigset_t pending;
};

signals::signals() : p(new impl) {
  sigemptyset(&p->members);
  sigemptyset(&p->pending);
}

signals::~signals() {}

void signals::add(int sig) {
  if (handlers.size() <= sig)
    handlers.resize(sig + 1);
  handlers[sig] = this;

  signal(sig, &common_handler);

  sigaddset(&p->members, sig);
}

void signals::ignore(int sig) {
  signal(sig, SIG_IGN);
  sigdelset(&p->members, sig);
}

void signals::block() {
  sigprocmask(SIG_BLOCK, &p->members, 0);
}

sigset_t const *signals::member_signals() const {
  return &p->members;
}

sigset_t const *signals::pending_signals() const {
  return &p->pending;
}

void signals::reset_pending() {
  sigemptyset(&p->pending);
}

bool signals::is_pending(int sig) const {
  return sigismember(&p->pending, sig);
}

void signals::handle(int sig) {
  sigaddset(&p->pending, sig);
}
