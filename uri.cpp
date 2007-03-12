// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"

namespace rest { namespace utils { namespace uri {

namespace {
int from_hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    return 0;
}

char to_hex(int c) {
  if (c < 10)
    return '0' + c;
  else
    return 'A' + c - 10;
}
}

std::string 
unescape(
    std::string::const_iterator begin, std::string::const_iterator end,
    bool form)
{
  std::string result;
  for (std::string::const_iterator it = begin; it != end; ++it)
    if (*it == '%' && it + 2 < end) {
      char code = char(from_hex(it[1]) * 16 + from_hex(it[2]));
      if (code != '\0')
        result += code;
      it += 2;
    } else if (form && *it == '+')
      result += ' ';
    else
      result += *it;
  return result;
}

std::string
escape(std::string::const_iterator begin, std::string::const_iterator end) {
  std::string result;
  for (std::string::const_iterator it = begin; it != end; ++it)
    if (0) { // must_escape
      unsigned char ch = *it;
      char buf[3] = { '%', to_hex(ch >> 4), to_hex(ch & 0xF) };
      result.append(buf, buf + 3);
    } else
      result += *it;
  return result;
}

}}}
