// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include "rest-config.hpp"

using namespace rest::utils;

void rest::utils::log(int priority, char const *message, ...) {
  static bool open = false;
  if (!open) {
    int opt = LOG_PID | LOG_CONS;
#ifndef NDEBUG
    opt |= LOG_PERROR;
#endif
    std::string const &name = rest::utils::get
      (rest::config::get().tree(), std::string("httpd(musikdings.rest)"),
       "general", "name");
    ::openlog(name.c_str(), opt, LOG_DAEMON);
    open = true;
  }
  va_list ap;
  va_start(ap, message);
  ::vsyslog(priority, message, ap);
}
