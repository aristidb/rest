// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_HTTP_HPP
#define REST_UTILS_HTTP_HPP

#include <time.h>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <cstddef>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/tuple/tuple.hpp>

namespace rest { class cookie; }

namespace rest { namespace utils { namespace http {

struct remote_close {};
struct bad_format {};

std::string random_boundary(std::size_t length = 50);

std::string datetime_string(time_t);
time_t datetime_value(std::string const &);

// checks if `c' is a space or a h-tab (see RFC 2616 chapter 2.2)
inline bool isspht(char c) {
  return c == ' ' || c == '\t';
}

template<class Source, typename Char>
bool expect(Source &in, Char c) {
  int t = boost::iostreams::get(in);
  if(t == c)
    return true;
  else if(t == EOF)
    throw remote_close();

  boost::iostreams::putback(in, t);
  return false;
}

template<class Source>
int remove_spaces(Source &in) {
  int c;
  do {
    c = boost::iostreams::get(in);
  } while(isspht(c));
  boost::iostreams::putback(in, c);
  return c;
}

typedef boost::tuple<std::string, std::string, std::string> request_line;
enum { REQUEST_METHOD, REQUEST_URI, REQUEST_HTTP_VERSION };

template<class Source>
void get_until(char end, Source &in, std::string &ret,
               bool allow_cr_or_lf = false,
               std::size_t max_length = 0) {
  int t;
  while ((t = boost::iostreams::get(in)) != end) {
    if(t == EOF)
      throw remote_close();
    if (!allow_cr_or_lf && (t == '\r' || t == '\n'))
      throw bad_format();
    if (max_length != 0 && ret.size() >= max_length)
      throw bad_format();
    ret += t;
  }
}

template<class Source>
request_line get_request_line(
    Source &in,
    boost::tuple<std::size_t, std::size_t, std::size_t> const &max_sizes =
      boost::make_tuple(0, 0, 0))
{
  while (expect(in, '\r'))
    if (!expect(in, '\n'))
      throw bad_format();

  request_line x;

  get_until(' ', in, x.get<REQUEST_METHOD>(), false, 
            max_sizes.get<REQUEST_METHOD>());
  if (x.get<REQUEST_METHOD>().empty())
    throw bad_format();

  get_until(' ', in, x.get<REQUEST_URI>(), false,
            max_sizes.get<REQUEST_URI>());
  if (x.get<REQUEST_URI>().empty())
    throw bad_format();

  get_until('\r', in, x.get<REQUEST_HTTP_VERSION>(), false,
            max_sizes.get<REQUEST_HTTP_VERSION>());
  if (x.get<REQUEST_HTTP_VERSION>().empty())
    throw bad_format();

  if (!expect(in, '\n'))
    throw bad_format();

  return x;
}

// reads a header field from `in' and adds it to `fields'
// see RFC 2616 chapter 4.2
// Warning: Field names are converted to all lower-case!
template<class Source, class HeaderFields>
void get_header_field(
    Source &in,
    HeaderFields &fields,
    std::size_t max_name_length,
    std::size_t max_value_length)
{
  namespace io = boost::iostreams;
  std::string name;

  get_until(':', in, name, false, max_name_length);
  boost::algorithm::to_lower(name);

  remove_spaces(in);

  std::string &value = fields[name];
  if (!value.empty())
    value += ", ";

  for (;;) {
    int t;
    if ((t = io::get(in)) == EOF)
      throw remote_close();
    if (t == '\n' || t == '\r') {
      // Newlines in header fields are allowed when followed
      // by an SP (space or horizontal tab)
      if (t == '\r')
        expect(in, '\n');
      t = io::get(in);
      if (!isspht(t)) {
        io::putback(in, t);
        break;
      }
      remove_spaces(in);
      value += ' ';
    } else {
      if (max_value_length != 0 && value.size() >= max_value_length)
        throw bad_format();
      value += t;
    }
  }

  std::string::reverse_iterator xend = value.rbegin();
  while (isspht(*xend))
    ++xend;
  value.erase(xend.base(), value.end());
}

template<class Source, class HeaderFields>
void read_headers(
    Source &source,
    HeaderFields &fields,
    std::size_t max_name_length = 0,
    std::size_t max_value_length = 0)
{
  for (;;) {
    get_header_field(source, fields, max_name_length, max_value_length);
    if (expect(source, '\r')) {
      if (!expect(source, '\n'))
        throw bad_format();
      return;
    }
  }
}

void parse_parametrised(
    std::string const &in,
    std::string &element,
    std::set<std::string> const &interesting_parameters,
    std::map<std::string, std::string> &parameters);

void parse_list(std::string const &in, std::vector<std::string> &out,
                char delimiter = ',', bool skip_empty = true);

void parse_qlist(std::string const &in, std::multimap<int, std::string> &out);

int parse_qvalue(std::string const &in);

void parse_cookie_header(std::string const &in,
                         std::vector<rest::cookie> &cookies);

}}}

#endif
