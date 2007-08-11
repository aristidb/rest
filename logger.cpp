// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include "rest-config.hpp"
#include <stdarg.h>

using namespace rest::utils;

void rest::utils::log(int priority, char const *message, ...) {
  static bool open = false;
  if (!open) {
    int opt = LOG_PID | LOG_CONS;
#ifdef DEBUG
    opt |= LOG_PERROR;
#endif
    rest::utils::property_tree &tree = rest::config::get().tree();
    std::string const &name = 
      rest::utils::get(tree, std::string(), "general", "name");
    assert(!name.empty());
    ::openlog(name.c_str(), opt, LOG_DAEMON);
    open = true;
  }
  va_list ap;
  va_start(ap, message);
  ::vsyslog(priority, message, ap);
}
