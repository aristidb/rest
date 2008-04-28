// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/signals.hpp"
#include <vector>

using rest::signals;

static std::vector<signals*> handlers;

static void common_handler(signals::signal_type sig) {
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

void signals::add(signal_type sig) {
  if (handlers.size() <= sig)
    handlers.resize(sig + 1);
  handlers[sig] = this;

  signal(sig, (void (*)(int)) &common_handler);

  sigaddset(&p->members, sig);
}

void signals::ignore(signal_type sig) {
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

bool signals::is_pending(signal_type sig) const {
  return sigismember(&p->pending, sig);
}

void signals::handle(signal_type sig) {
  sigaddset(&p->pending, sig);
}
