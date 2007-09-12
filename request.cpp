// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include <map>
#include <boost/none.hpp>

using rest::request;

class request::impl {
public:
  typedef std::map<std::string, std::string> header_map;
  header_map headers;
};

request::request() : p(new impl) {}
request::~request() {}

void request::clear() {
  p->headers.clear();
}

void request::read_headers(std::streambuf &buf) {
  utils::http::read_headers(buf, p->headers);
}

boost::optional<std::string> request::get_header(std::string const &name) const
{
  impl::header_map::iterator it = p->headers.find(name);
  if (it == p->headers.end())
    return boost::none;
  else
    return it->second;
}

void request::erase_header(std::string const &name) {
  p->headers.erase(name);
}
