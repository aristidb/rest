// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/https.hpp"

#include <boost/shared_ptr.hpp>

using rest::https_scheme;

class https_scheme::impl {
 public:
  impl()
  {
  }
};

https_scheme::https_scheme()
  : p(new impl())
{ }

https_scheme::~https_scheme() { }

std::string const &https_scheme::name() const {
  static std::string x("https");
  return x;
}

boost::any https_scheme::create_context(
  utils::property_tree const &socket_data) const
{
  boost::shared_ptr<tls::dh_params> dh = new tls::dh_params(utils::get(socket_data, 2048, "tls", "dh_bits")); // TODO
  char const * const cafile = utils::get(socket_data, (char const*)0x0, "tls", "cafile");
  char const * const crlfile = utils::get(socket_data, (char const*)0x0, "tls", "crlfile");
  char const * const certfile = utils::get(socket_data, (char const*)0x0, "tls", "certfile");
  char const * const keyfile = utils::get(socket_data, (char const*)0x0, "tls", "keyfile");
  if(!cafile || !crlfile || !certfile || !keyfile)
    throw utils::error("cafile, crlfile, certfile or keyfile not set");
  boost::shared_ptr<tls::x509_certificate_credentials> cred = new tls::x509_certificate_credentials(cafile, crlfile, certfile, keyfile, dh);
  boost::shared_ptr<tls::priority> prio = new tls::priority( utils::get(socket_data, "NORMAL", "tls", "priority"));

  return boost::make_tuple(dh, cred, prio);
}

void https_scheme::serve(
  logger *log,
  int connfd,
  socket_param const &sock,
  network::address const &addr,
  std::string const &servername)
{
  // TODO: timeout with setsockopt...


  // hier auch gucken
  boost::any const &scheme_specific = sock.scheme_specific();

  http_connection conn(sock.hosts(), addr, servername, log);
  tls::session session(cred, prio, connfd); // TODO cred, prio
  std::auto_ptr<std::streambuf> p(new tls::stream_buffer(session));

  conn.serve(p);
}

// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
