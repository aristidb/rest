// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <vector>
#include <utility>

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

context &host::get_context() { return p->con; }

void host::do_store(void *x, void (*destruct)(void *))  {
  p->storage.push_back(std::make_pair(x, destruct));
}
