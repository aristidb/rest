// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/http.hpp"
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
  typedef std::pair<iterator, iterator> token;

  unsigned num(token const &t) {
    unsigned ret = 0;
    iterator i = t.first;
    for (; i != t.second; ++i)
      if (*i >= '0' && *i <= '9')
        ret = ret * 10 + *i - '0';
    return ret;
  }

#define MON(x,y,z) ((x - 'A') + (y - 'a') + (z - 'a') - 9)

  // month        = "Jan" | "Feb" | "Mar" | "Apr"
  //                  | "May" | "Jun" | "Jul" | "Aug"
  //                  | "Sep" | "Oct" | "Nov" | "Dec"
  int mon(token const &t) {
    if(t.second - t.first != 3)
      return -1;
    switch (MON(*t.first, *(t.first + 1), *(t.first + 2))) {
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
    default: return -1;
    }
  }

  // wkday        = "Mon" | "Tue" | "Wed"
  //                    | "Thu" | "Fri" | "Sat" | "Sun"
  int wkday(token const &t) {
    if(t.second - t.first != 3)
      return -1;
    switch (MON(*t.first, *(t.first + 1), *(t.first + 2))) {
    case MON('M', 'o', 'n'): return 0;
    case MON('T', 'u', 'e'): return 1;
    case MON('W', 'e', 'd'): return 2;
    case MON('T', 'h', 'u'): return 3;
    case MON('F', 'r', 'i'): return 4;
    case MON('S', 'a', 't'): return 5;
    case MON('S', 'u', 'n'): return 6;
    default: return -1;
    }
  }
#undef MON
  bool operator==(token const &tok, char const *rhs) {
    iterator i = tok.first;
    while( i != tok.second && *rhs )
      if( *i++ != *rhs++ )
        return false;
    return i == tok.second && !*rhs;
  }
  bool operator!=(token const &tok, char const *rhs) {
    return !(tok == rhs);
  }

#if 0
  std::ostream &operator<<(std::ostream &out, token const &t) {
    iterator i = t.first;
    while(i != t.second)
      out << *i++;
    return out;
  }
#endif

  bool gettoken(std::string const &txt, token &tok) {
    iterator begin = tok.second;
    if(begin == txt.end()) {
      tok = std::make_pair(begin, begin);
      return false;
    }

    if(isspace(*begin))
      while(isspace(*begin))
        ++begin;
    else if(*begin == ',' || *begin == '-' || *begin == ':') {
      tok = std::make_pair(begin, begin + 1);
      return true;
    }

    iterator i = begin;
    for(;i != txt.end(); ++i) {
      if(isalnum(*i))
        ;
      else if(isspace(*i) || *i == ',' || *i == '-' || *i == ':')
        break;
      else {
        // error!
        return false;
      }
    }

    tok = std::make_pair(begin, i);
    return true;
  }

  // time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
  //                    ; 00:00:00 - 23:59:59
  bool time(std::string const &text, token &t, tm &data) {
    if(t.second - t.first > 2)
      return false;
    data.tm_hour = num(t);

    gettoken(text, t);
    if(*t.first != ':')
      return false;

    gettoken(text, t);
    if(t.second - t.first > 2)
      return false;
    data.tm_min = num(t);

    gettoken(text, t);
    if(*t.first != ':')
      return false;

    gettoken(text, t);
    if(t.second - t.first > 2)
      return false;
    data.tm_sec = num(t);

    gettoken(text, t);
    return true;
  }

  // date1        = 2DIGIT SP month SP 4DIGIT
  //                    ; day month year (e.g., 02 Jun 1982)
  bool date1(std::string const &text, token &t, tm &data) {
    if(t.second - t.first > 2)
      return false;
    data.tm_mday = num(t);
    
    gettoken(text, t);
    data.tm_mon = mon(t);
    if(data.tm_mon == -1)
      return false;

    gettoken(text, t);
    data.tm_year = num(t) - 1900;

    gettoken(text, t);
    return true;
  }

  // rfc1123-date = wkday "," SP date1 SP time SP "GMT"
  bool rfc1123_date(std::string const &text, token t, tm &data) {
    if(wkday(t) == -1)
      return false;

    gettoken(text, t);
    if(*t.first != ',')
      return false;

    gettoken(text, t);
    if(!date1(text, t, data))
      return false;
    if(!time(text, t, data))
      return false;
    if(t != "GMT")
      return false;
    return true;
  }

  // weekday      = "Monday" | "Tuesday" | "Wednesday"
  //                | "Thursday" | "Friday" | "Saturday" | "Sunday"
  bool weekday(token const &t) {
    return
      t == "Monday" || t == "Tuesday" || t == "Wednesday" ||
      t == "Thursday" || t == "Friday" || t == "Saturday" || t == "Sunday";
  }

  // date2        = 2DIGIT "-" month "-" 2DIGIT
  //                    ; day-month-year (e.g., 02-Jun-82)
  bool date2(std::string const &text, token &t, tm &data) {
    if(t.second - t.first > 2)
      return false;
    data.tm_mday = num(t);

    gettoken(text, t);
    if(*t.first != '-')
      return false;

    gettoken(text, t);
    data.tm_mon = mon(t);
    if(data.tm_mon == -1)
      return false;

    gettoken(text, t);
    if(*t.first != '-')
      return false;

    gettoken(text, t);
    if(t.second - t.first > 2)
      return false;
    data.tm_year = num(t);
    if(data.tm_year < 60) // everything below 60 ist 20xx, everything above 19xx
      data.tm_year += 100;

    gettoken(text, t);
    return true;
  }

  // rfc850-date  = weekday "," SP date2 SP time SP "GMT"
  bool rfc850_date(std::string const &text, token t, tm &data) {
    if(!weekday(t))
      return false;

    gettoken(text, t);
    if(*t.first != ',')
      return false;

    gettoken(text, t);
    if(!date2(text, t, data))
      return false;
    if(!time(text, t, data))
      return false;
    if(t != "GMT")
      return false;
    return true;
  }

  // date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
  //                    ; month day (e.g., Jun  2)
  bool date3(std::string const &text, token &t, tm &data) {
    data.tm_mon = mon(t);
    if(data.tm_mon == -1)
      return false;

    gettoken(text, t);
    data.tm_mday = num(t);

    gettoken(text, t);
    return true;
  }

  // asctime-date = wkday SP date3 SP time SP 4DIGIT
  bool asctime_date(std::string const &text, token t, tm &data) {
    if(wkday(t) == -1)
      return false;

    gettoken(text, t);
    if(!date3(text, t, data))
      return false;
    if(!time(text, t, data))
      return false;
    data.tm_year = num(t) - 1900;
    return true;
  }
}

//     Must understand at least these formats:
//       Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
//       Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
//       Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
//
//       HTTP-date    = rfc1123-date | rfc850-date | asctime-date
time_t rest::utils::http::datetime_value(std::string const &text) {  
  tm out;
  token t = std::make_pair(text.begin(), text.begin());
  gettoken(text, t);
  if(rfc1123_date(text, t, out) ||
     rfc850_date(text, t, out) ||
     asctime_date(text, t, out))
    return timegm(&out);
  return -1;
}
