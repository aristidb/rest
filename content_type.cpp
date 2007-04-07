// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <boost/algorithm/string.hpp>
#include <cctype>

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
  iterator type_end = param_left;
  while (type_end != begin) {
    if (!std::isspace(type_end[-1]))
      break;
    --type_end;
  }

  type.assign(begin, type_end);
  algo::to_lower(type);

  for (iterator it = param_left; it != end;) {
    while (++it != end)
      if (!std::isspace(*it))
        break;
    iterator next = std::find(it, end, ';');
    iterator delim = std::find(it, next, '=');
    std::string key(it, delim);
    algo::to_lower(key);
    std::string value;
    if (delim != next)
      ++delim;
    if (delim == next || *delim != '"') {
      iterator qend = next;
      while (qend != delim) {
        if (!std::isspace(qend[-1]))
          break;
        --qend;
      }
      value.assign(delim, next);
    } else {
      ++delim;
      value.reserve(next - delim);
      while (delim != end && *delim != '"') {
        if (*delim == '\\') {
          ++delim;
          if (delim == end)
            break;
        }
        value += *delim;
        ++delim;
      }
      next = std::find(delim, end, ';');
    }
    if (interesting_parameters.count(key) != 0)
      parameters.insert(std::make_pair(key, value));
    it = next;
  }
}

