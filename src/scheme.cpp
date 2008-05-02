// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/scheme.hpp"
#include "rest/http_connection.hpp"
#include "rest/socket_param.hpp"
#include "rest/utils/socket_device.hpp"
#include <boost/iostreams/stream_buffer.hpp>

using rest::scheme;
using rest::http_scheme;

std::string const &scheme::type_name() {
  static std::string x("scheme");
  return x;
}

bool scheme::need_load_standard_objects = true;

void scheme::load_standard_objects(object_registry &obj_reg) {
  std::auto_ptr<object> http_o(new http_scheme);
  obj_reg.add(http_o);

  need_load_standard_objects = false;
}

scheme::~scheme() {}


std::string const &http_scheme::name() const {
  static std::string x("http");
  return x;
}

http_scheme::~http_scheme() {}

void http_scheme::serve(
  logger *log,
  int connfd,
  socket_param const &sock,
  network::address const &addr,
  std::string const &servername)
{
  namespace io = boost::iostreams;

  http_connection conn(sock.hosts(), addr, servername, log);
  utils::socket_device dev(connfd, sock.timeout_read(), sock.timeout_write());
  std::auto_ptr<std::streambuf> p(new io::stream_buffer<utils::socket_device>(dev));

  conn.serve(p);
}
