// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/logger.hpp"
#include "rest/config.hpp"
#include <stdarg.h>

using rest::logger;
using rest::plaintext_logger;

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

class plaintext_logger::impl {
public:
};

plaintext_logger::plaintext_logger(priority min_priority)
: logger(min_priority), p(new impl)
{}

plaintext_logger::~plaintext_logger()
{}

void plaintext_logger::do_flush() {
}

void plaintext_logger::do_log(priority p, std::string const &f, std::string const &v) {
}
