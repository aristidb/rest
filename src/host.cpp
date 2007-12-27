// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/host.hpp"
#include "rest/context.hpp"
#include "rest/response.hpp"
#include <vector>
#include <utility>
#include <sstream>
#include <map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <stdexcept>

using namespace rest;

class host::impl {
public:
  std::string name;
  context con;

  typedef std::vector<std::pair<void *, void (*)(void *)> > storage_t;
  storage_t storage;

  typedef std::map<int, std::pair<std::string, std::string> > std_resp_t;
  std_resp_t standard_responses;

  boost::function<void (response &)> response_preparer;

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

typedef
  boost::multi_index_container<
    boost::reference_wrapper<host const>,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        boost::multi_index::const_mem_fun<host, std::string, &host::get_host>
      >
    >
  >
  hosts_cont_t;

class host_container::impl {
public:
  hosts_cont_t hosts;
};

host_container::host_container() : p(new impl) {}
host_container::~host_container() {}

void host_container::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

host const *host_container::get_host(std::string const &name) const {
  std::string::const_iterator begin = name.begin();
  std::string::const_iterator end = name.end();
  std::string::const_iterator delim = std::find(begin, end, ':');

  std::string the_host(begin, delim);

  hosts_cont_t::const_iterator it = p->hosts.find(the_host);
  while(it == p->hosts.end() &&
        !the_host.empty())
  {
    std::string::const_iterator begin = the_host.begin();
    std::string::const_iterator end = the_host.end();
    std::string::const_iterator delim = std::find(begin, end, '.');

    if (delim == end)
      the_host.clear();
    else
      the_host.assign(++delim, end);

    it = p->hosts.find(the_host);
  }
  if(it == p->hosts.end())
    return 0x0;
  return it->get_pointer();
}

void host::set_response_preparer(boost::function<void (response &)> const &f) {
  p->response_preparer = f;
}

void host::prepare_response(response &r) const {
  if (p->response_preparer)
    p->response_preparer(r);
}
