// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LOG_HPP
#define REST_UTILS_LOG_HPP

#include <syslog.h>

namespace rest { namespace utils {

void log(int priority, char const *message, ...);

}}

#endif
