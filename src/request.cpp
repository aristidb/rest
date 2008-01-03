// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/request.hpp"
#include "rest/headers.hpp"
#include "rest/host.hpp"
#include "rest/utils/http.hpp"
#include "rest/utils/string.hpp"
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

  network::address addr;

  headers headers_;
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
  p.reset(new impl(p->addr));
}

rest::headers &request::get_headers() {
  return p->headers_;
}
