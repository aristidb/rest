// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>

#ifndef REST_LOGPIPE
#define REST_LOGPIPE "rest-logpipe"
#endif

using namespace rest::utils;

class logger::impl {
public:
  int fd;
  int min_priority;
};

logger::logger() : p(new impl) {
  if (::mkfifo(REST_LOGPIPE, 0644) == -1)
    if (errno != EEXIST)
      throw errno_error("mkfifo");
  p->fd = open(REST_LOGPIPE, O_RDWR);//TODO: decide if this should be O_WRONLY
  ::fcntl(p->fd, F_SETFD, FD_CLOEXEC);
  if(p->fd == -1)
    throw errno_error("open[fifo]");
  p->min_priority = INFO;
}

static logger *instance;

void logger::init() {
  if (!instance)
    instance = new logger;
}

logger &logger::get() {
  init();
  return *instance;
}

void logger::set_minimum_priority(int priority) {
  p->min_priority = priority;
}

namespace {
  char const *const priority_string[] = {
    "DEBUG", "INFO", "IMPORTANT", "CRITICAL"
  };
}

void logger::log(
    int priority,
    char const *file,
    char const *func,
    int line,
    std::string const &data)
{
  if (p->min_priority > priority)
    return;
  // may be we should pipe binary data and pipedump should fix it?
  std::stringstream out;
  out << '[' << std::time(0x0) << "] ";
  if(priority == 0)
    out << priority_string[0];
  else if(priority > 0 && priority < 90)
    out << priority_string[1];
  else if(priority >= 90 && priority < 100)
    out << priority_string[2];
  else if(priority >= 100)
    out << priority_string[3];

  out << "@(" << file << ':' << func << ':' << line << "): `" << data << "'\n";

  std::cerr << "LOG: " << out.str();
  char const *ptr = out.str().c_str();
  size_t wrote = 0;
  do {
    ssize_t n = write(p->fd, ptr + wrote, out.str().length() - wrote);
    if(n < 0) {
      // horrible situation!
      std::cerr << "CRITICAL: writing to log failed\n";
    }
    else
      wrote += n;
  } while(wrote < out.str().length());
}

