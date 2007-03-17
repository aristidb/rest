// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <boost/thread/once.hpp>
#include <testsoon.hpp>

#ifndef REST_LOGPIPE
#define REST_LOGPIPE "/tmp/rest-logpipe"
#endif

using namespace rest::utils;

class logger::impl {
public:
};

logger::logger() : p(new impl) {}

static logger *instance;
static boost::once_flag once = BOOST_ONCE_INIT;

void logger::init() {
    instance = new logger;
}

logger &logger::get() {
  boost::call_once(&init, once);
  return *instance;
}

TEST() {
  Not_equals(&logger::get(), (logger*) 0);
}

TEST() {
  logger *l1 = &logger::get();
  logger *l2 = &logger::get();
  Equals(l1, l2);
}

