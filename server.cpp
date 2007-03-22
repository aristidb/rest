// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/ref.hpp>

using namespace rest;
using namespace boost::multi_index;

class server::impl {
public:
  short port;

  typedef
    boost::multi_index_container<
      boost::reference_wrapper<host const>,
      indexed_by<
        hashed_unique<
          const_mem_fun<host, std::string, &host::get_host>
        >
      >
    >
    cont_t;

  cont_t hosts;

  impl(short port) : port(port) {}
};

server::server(short port) : p(new impl(port)) {}
server::~server() {}

void server::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

void server::serve() {
  // TODO
}
