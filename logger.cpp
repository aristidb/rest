// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <boost/thread/once.hpp>
#include <testsoon.hpp>
#include <signal.h> // for sig_atomic_t

#ifndef REST_LOGPIPE
#define REST_LOGPIPE "/tmp/rest-logpipe"
#endif

using namespace rest::utils;

class logger::impl {
public:
  int fd;
  sig_atomic_t min_priority;
};

logger::logger() : p(new impl) {
  p->fd = 0;
  p->min_priority = INFO;
}

static logger *instance;
static boost::once_flag once = BOOST_ONCE_INIT;

void logger::init() {
    instance = new logger;
}

logger &logger::get() {
  boost::call_once(&init, once);
  return *instance;
}

void logger::set_minimum_priority(int priority) {
  p->min_priority = priority;
}

void logger::log(
    int priority,
    char const *file,
    int line,
    std::string const &data)
{
  if (p->min_priority > priority)
    return;
  (void) file; (void) line; (void) data; // TODO
}

TEST() {
  Not_equals(&logger::get(), (logger*) 0);
}

TEST() {
  logger *l1 = &logger::get();
  logger *l2 = &logger::get();
  Equals(l1, l2);
}

