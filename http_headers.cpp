// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <boost/algorithm/string.hpp>
#include <cctype>

namespace algo = boost::algorithm;

namespace {
template<class It>
void skip_ws_fwd(It &it, It end) {
  for (; it != end && std::isspace(*it); ++it)
    ;
}

template<class It>
void skip_ws_bwd(It &it, It begin) {
  for (; it != begin && std::isspace(it[-1]); --it)
    ;
}
}

void rest::utils::http::parse_parametrised(
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
  skip_ws_bwd(type_end, begin);

  type.assign(begin, type_end);
  algo::to_lower(type);

  for (iterator it = param_left; it != end;) {
    skip_ws_fwd(++it, end);
    iterator next = std::find(it, end, ';');
    iterator delim = std::find(it, next, '=');
    std::string key(it, delim);
    algo::to_lower(key);
    std::string value;
    if (delim != next)
      ++delim;
    if (delim == next || *delim != '"') {
      iterator qend = next;
      skip_ws_bwd(qend, delim);
      value.assign(delim, qend);
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

void rest::utils::http::parse_list(
    std::string const &in, std::vector<std::string> &out)
{
  typedef std::string::const_iterator iterator;

  iterator it = in.begin();
  iterator end = in.end();

  skip_ws_fwd(it, end);
  skip_ws_bwd(end, it);

  while (it != end) {
    iterator delim = std::find(it, end, ',');
    iterator left = delim;
    skip_ws_bwd(left, it);
    if (it != left)
      out.push_back(std::string(it, left));
    it = delim;
    if (it != end)
      ++it;
    skip_ws_fwd(it, end);
  }
}

