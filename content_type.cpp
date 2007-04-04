// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <boost/algorithm/string.hpp>

namespace algo = boost::algorithm;

void rest::utils::parse_content_type(
    std::string const &in,
    std::string &type,
    std::set<std::string> const &interesting_parameters,
    std::map<std::string, std::string> &parameters)
{
  typedef std::string::const_iterator iterator;

  iterator begin = in.begin();
  iterator end = in.end();

  iterator param_left = std::find(begin, end, ';');
  while (param_left != begin) {
    if (param_left[-1] != ' ')
      break;
    --param_left;
  }

  type.assign(begin, param_left);
  algo::to_lower(type);

  for (iterator it = param_left; it != end;) {
    while (++it != end)
      if (*it != ' ')
        break;
    iterator next = std::find(it, end, ';');
    iterator delim = std::find(it, next, '=');
    std::string key(it, delim);
    std::string value;
    if (delim != next)
      ++delim;
    if (delim == next || *delim != '"')
      value.assign(delim, next);
    else {
      ++delim;
      value.reserve(next - delim);
      while (delim != next && *delim != '"') {
        if (*delim == '\\') {
          if (++delim == next) {
            if (next == end)
              break;
            next = std::find(next, end, ';');
          }
        }
        value += *delim;
        ++delim;
      }
    }
    if (interesting_parameters.count(key) != 0)
      parameters.insert(std::make_pair(key, value));
    it = next;
  }
}

