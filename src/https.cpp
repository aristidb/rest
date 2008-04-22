// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/https.hpp"

using rest::https_scheme;

https_scheme::~https_scheme() {}

std::string const &https_scheme::name() const {
  static std::string x("https");
  return x;
}

void https_scheme::serve(
  logger *log,
  int connfd,
  socket_param const &sock,
  network::address const &addr,
  std::string const &servername)
{
  // TODO
}
