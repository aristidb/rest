// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/logger.hpp"
#include "rest/config.hpp"
#include <iostream>
#include <sstream>

using rest::logger;
using rest::plaintext_logger;

class plaintext_logger::impl {
public:
  std::ostringstream message;
};

plaintext_logger::plaintext_logger(priority min_priority)
: logger(min_priority), p(new impl)
{
  flush();
}

plaintext_logger::~plaintext_logger() {
  flush();
}

void plaintext_logger::do_flush() {
  if (p->message.str().empty())
    return;

  p->message << "{" << getpid() << '/' << time(0) << "}\n\n";
  std::cerr << p->message.str();
  p->message.str(std::string());
}

void plaintext_logger::do_log(priority prio, std::string const &f, std::string const &v) {
  p->message << get_sequence_number() << ' ' << prio << " [" << f << "]";
  if (!v.empty())
    p->message << " = [" << v << "]";
  p->message << '\n';
}
