// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cassert>
#include <algorithm>

std::string rest::utils::http::datetime_string(time_t time_buf) {
  tm gmtime;
  gmtime_r(&time_buf, &gmtime);

  std::stringstream out;
  switch (gmtime.tm_wday) {
  case 0: out << "Sun"; break;
  case 1: out << "Mon"; break;
  case 2: out << "Tue"; break;
  case 3: out << "Wed"; break;
  case 4: out << "Thu"; break;
  case 5: out << "Fri"; break;
  case 6: out << "Sat"; break;
  }

  out << ", ";
  out << std::setfill('0');
  out << std::setw(2) << gmtime.tm_mday << ' ';

  switch (gmtime.tm_mon) {
  case 0: out << "Jan"; break;
  case 1: out << "Feb"; break;
  case 2: out << "Mar"; break;
  case 3: out << "Apr"; break;
  case 4: out << "May"; break;
  case 5: out << "Jun"; break;
  case 6: out << "Jul"; break;
  case 7: out << "Aug"; break;
  case 8: out << "Sep"; break;
  case 9: out << "Oct"; break;
  case 10: out << "Nov"; break;
  case 11: out << "Dec"; break;
  }

  out << ' ';
  out << (1900 + gmtime.tm_year);
  out << ' ';

  out << std::setw(2) << gmtime.tm_hour << ':';
  out << std::setw(2) << gmtime.tm_min << ':';
  out << std::setw(2) << gmtime.tm_sec;

  out << " GMT";
  return out.str();
}

namespace {
  typedef std::string::const_iterator iterator;

  unsigned num(iterator first, iterator last) {
    unsigned ret = 0;
    for (; first != last; ++first)
      if (*first >= '0' && *first <= '9')
        ret = ret * 10 + *first - '0';
    return ret;
  }

#define MON(x,y,z) ((x) + 0x100 * (y) + 0x200 * (z))

  unsigned mon(iterator first, iterator last) {
    assert(first + 3 == last);
    switch (MON(*first, *(first + 1), *(first + 2))) {
    case MON('J', 'a', 'n'): return 0;
    case MON('F', 'e', 'b'): return 1;
    case MON('M', 'a', 'r'): return 2;
    case MON('A', 'p', 'r'): return 3;
    case MON('M', 'a', 'y'): return 4;
    case MON('J', 'u', 'n'): return 5;
    case MON('J', 'u', 'l'): return 6;
    case MON('A', 'u', 'g'): return 7;
    case MON('S', 'e', 'p'): return 8;
    case MON('O', 'c', 't'): return 9;
    case MON('N', 'o', 'v'): return 10;
    case MON('D', 'e', 'c'): return 11;
    }
  }
#undef MON
}

time_t rest::utils::http::datetime_value(std::string const &text) {
  /*
    Must understand at least these formats:
      Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
      Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
      Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
  */
  static char const sample822[] = "Sun, 06 Nov 1994 08:49:37 GMT";
  static char const sample850[] = "Sunday, 06-Nov-94 08:49:37 GMT";
  static char const sampleASC[] = "Sun Nov  6 08:49:37 1994";

  tm out;

  switch (text.length() + 1) {
  case sizeof(sample822):
    if (!equal(text.end() - 3, text.end(), "GMT"))
      return -1;
    out.tm_mday = num(text.begin() + 5, text.begin() + 7);
    out.tm_mon = mon(text.begin() + 8, text.begin() + 11);
    out.tm_year = num(text.begin() + 12, text.begin() + 16) - 1900;
    out.tm_hour = num(text.begin() + 17, text.begin() + 19);
    out.tm_min = num(text.begin() + 20, text.begin() + 22);
    out.tm_sec = num(text.begin() + 23, text.begin() + 25);
    break;
  case sizeof(sample850):
    if (!equal(text.end() - 3, text.end(), "GMT"))
      return -1;
    break;
  case sizeof(sampleASC):
    break;
  default:
    return time_t(-1);
  }

  return mktime(&out);//XXX localtime, must be gmt
}
