// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/context.hpp"
#include "rest/rest.hpp"
#include <vector>
#include <utility>
#include <sstream>

using namespace rest;

class host::impl {
public:
  std::string name;
  context con;

  typedef std::vector<std::pair<void *, void (*)(void *)> > storage_t;
  storage_t storage;

  typedef std::map<int, std::pair<std::string, std::string> > std_resp_t;
  std_resp_t standard_responses;

  impl(std::string const &name) : name(name) {}

  ~impl() {
    for (storage_t::iterator it = storage.begin(); it != storage.end(); ++it)
      it->second(it->first);
  }
};

host::host(std::string const &name) : p(new impl(name)) {
  for (int code = 0; code < 1000; ++code) {
    if (code == 200)
      continue;
    char const *reason = response::reason(code);
    if (!*reason)
      continue;
    std::ostringstream data;
    data << reason << " (Code " << code << ")\n";
    set_standard_response(code, "text/plain", data.str());
  }
}

host::~host() {}

void host::set_host(std::string const &name) { p->name = name; }

std::string host::get_host() const { return p->name; }

context &host::get_context() const { return p->con; }

void host::do_store(void *x, void (*destruct)(void *))  {
  p->storage.push_back(std::make_pair(x, destruct));
}

void host::make_standard_response(response &resp) const {
  int const code = resp.get_code();
  impl::std_resp_t::const_iterator it = p->standard_responses.find(code);

  if (it == p->standard_responses.end())
    return;

  resp.set_type(it->second.first);
  resp.set_data(it->second.second);
}

void host::set_standard_response(
  int code, std::string const &mime, std::string const &text)
{
  p->standard_responses.erase(code);
  p->standard_responses.insert(
    std::make_pair(code, std::make_pair(mime, text)));
}
