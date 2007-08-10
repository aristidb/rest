// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"

using namespace rest::utils;

void rest::utils::log(int priority, char const *message, ...) {
  static bool open = false;
  if (!open) {
    int opt = LOG_PID | LOG_CONS;
#ifndef NDEBUG
    opt |= LOG_PERROR;
#endif
    openlog("httpd(musikdings.rest)", opt, LOG_DAEMON);
    open = true;
  }
  va_list ap;
  va_start(ap, message);
  vsyslog(priority, message, ap);
}
