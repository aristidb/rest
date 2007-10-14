// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include "rest.hpp"

#include <cctype>
#include <boost/algorithm/string.hpp>

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

    iterator right = delim;
    skip_ws_bwd(right, it);
    std::string key(it, right);

    algo::to_lower(key);
    std::string value;
    if (delim != next)
      ++delim;
    skip_ws_fwd(delim, next);
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
  std::string const &in, std::vector<std::string> &out, char delimiter)
{
  typedef std::string::const_iterator iterator;

  iterator it = in.begin();
  iterator end = in.end();

  skip_ws_fwd(it, end);
  skip_ws_bwd(end, it);

  while (it != end) {
    iterator delim = std::find(it, end, delimiter);
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

int rest::utils::http::parse_qvalue(std::string const &in) {
  if (in.empty())
    return 1000;
  int x = 1000;
  int v = 0;
  for (std::string::const_iterator it = in.begin(); it != in.end(); ++it) {
    if (*it >= '0' && *it <= '9') {
      v += (*it - '0') * x;
      x /= 10;
    }
    else if(*it == '.' && x == 1000)
      x = 100;
  }
  return v;
}

void rest::utils::http::parse_qlist(
    std::string const &in, std::multimap<int, std::string> &out)
{
  std::vector<std::string> tmp;
  parse_list(in, tmp);

  std::set<std::string> q;
  q.insert("q");

  for (std::vector<std::string>::iterator it=tmp.begin(); it!=tmp.end(); ++it) {
    std::string self;
    std::map<std::string, std::string> param;
    parse_parametrised(*it, self, q, param);
    out.insert(std::make_pair(parse_qvalue(param["q"]), self));
  }
}

namespace {
  typedef std::pair<std::string, std::string> name_value_pair;
  name_value_pair parse_name_value_pair(std::string const &in) {
    typedef std::string::const_iterator iterator;
    iterator begin = in.begin();
    iterator end = in.end();

    skip_ws_fwd(begin, end);
    skip_ws_bwd(end, begin);

    iterator const delim = std::find(begin, end, '=');
    if(delim == end)
      return std::make_pair(std::string(begin, end), std::string());

    iterator end_name = delim;
    skip_ws_bwd(end_name, begin);

    iterator begin_value = delim + 1;
    skip_ws_fwd(begin_value, end);

    return std::make_pair(std::string(begin, end_name),
                          std::string(begin_value, end));
  }
}

void rest::utils::http::parse_cookie_header(std::string const &in,
                                            std::vector<rest::cookie> &cookies)
{
  typedef std::vector<rest::cookie>::iterator cookie_iterator;
  typedef std::vector<std::string>::const_iterator iterator;

  std::vector<std::string> cookie_values;
  parse_list(in, cookie_values, ',');

  iterator const end = cookie_values.end();
  for(iterator i = cookie_values.begin(); i != end; ++i) {
    std::vector<std::string> params;
    parse_list(*i, params, ';');
    iterator const params_end = params.end();
    for(iterator j = params.begin(); j != params_end; ++j) {
      name_value_pair nv = parse_name_value_pair(*j);
      algo::to_lower(nv.first);
      if(nv.first[0] == '$') { // Parameter start with $
        if(!cookies.empty()) {
          if(nv.first == "$path")
            cookies.back().path = nv.second;
          else if(nv.first == "$domain")
            cookies.back().domain = nv.second;          
        }
        // ignore $version
      }
      else {
        cookies.push_back(rest::cookie(nv.first, nv.second));
      }
    }
  }
}

