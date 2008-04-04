// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/log.hpp"
#include "rest/config.hpp"
#include <stdarg.h>

using rest::utils::logger;
using rest::utils::record_logger;

logger::~logger() {}

record_logger *logger::create_record() {
  record_logger *rec = do_create_record();
  return rec;
}


record_logger::~record_logger() {}

void record_logger::add(std::string const &field, std::string const &value) {
  do_add(field, value);
}

void record_logger::flush() {
  do_flush();
}

void record_logger::close() {
  delete this;
}


void rest::utils::log(int priority, char const *message, ...) {
  static bool open = false;
  if (!open) {
    int opt = LOG_PID | LOG_CONS;
#ifdef DEBUG
    opt |= LOG_PERROR;
#endif
    property_tree &tree = config::get().tree();
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
