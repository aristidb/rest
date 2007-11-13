// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_NETWORK_HPP
#define REST_NETWORK_HPP

#include <string>
#include <boost/cstdint.hpp>

namespace rest { namespace network {

typedef union {
  boost::uint32_t ip4;
  boost::uint64_t ip6[2];
} addr_t;

struct address {
  int type;
  addr_t addr;
};

std::string ntoa(address const &addr);

}}

#endif
