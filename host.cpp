// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
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

  impl(std::string const &name) : name(name) {}

  ~impl() {
    for (storage_t::iterator it = storage.begin(); it != storage.end(); ++it)
      it->second(it->first);
  }
};

host::host(std::string const &name) : p(new impl(name)) {}

host::~host() {}

void host::set_host(std::string const &name) { p->name = name; }

std::string host::get_host() const { return p->name; }

context &host::get_context() const { return p->con; }

void host::do_store(void *x, void (*destruct)(void *))  {
  p->storage.push_back(std::make_pair(x, destruct));
}

void host::make_standard_response(response &resp) const {
  if (resp.get_code() == -1 || resp.get_code() == 200)
    return;

  //TODO?
  resp.set_type("text/plain");

  int const code = resp.get_code();

  std::ostringstream data;
  data << response::reason(code) << " (HTTP " << code << ")\n";

  resp.set_data(data.str());
}
