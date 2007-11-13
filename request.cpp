// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/request.hpp"
#include "rest/host.hpp"
#include "rest/utils.hpp"
#include <map>
#include <boost/none.hpp>

using rest::request;

namespace {
  static rest::host dummy_host("");
}

class request::impl {
public:
  impl(network::address const &addr) : host_(&dummy_host), addr(addr) { }

  std::string uri;

  host const *host_;

  typedef std::map<std::string, std::string,
                   rest::utils::string_icompare>
          header_map;

  header_map headers;

  network::address addr;
};

request::request(network::address const &addr) : p(new impl(addr)) { }
request::~request() {}

void request::set_uri(std::string const &uri) {
  p->uri = uri;
}

std::string const &request::get_uri() const {
  return p->uri;
}

void request::set_host(host const &h) {
  p->host_ = &h;
}

rest::host const &request::get_host() const {
  return *p->host_;
}

rest::network::address const &request::get_client_address() const {
  return p->addr;
}

void request::clear() {
  p->uri = std::string();
  p->host_ = 0;
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

void request::for_each_header(header_callback const &cb) const {
  for (impl::header_map::iterator it = p->headers.begin();
      it != p->headers.end();
      ++it)
    cb(it->first, it->second);
}
