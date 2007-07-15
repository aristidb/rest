// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

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

time_t rest::utils::http::datetime_value(std::string const &text) {
  return time_t(-1);
}
