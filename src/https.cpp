// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/https.hpp"
#include "rest/tls.hpp"
#include "rest/config.hpp"
#include "rest/socket_param.hpp"
#include "rest/http_connection.hpp"
#include <boost/shared_ptr.hpp>

using rest::https_scheme;

class https_scheme::impl {
public:
  impl()
  {
  }

  struct context {
    boost::shared_ptr<tls::dh_params> dh;
    boost::shared_ptr<tls::x509_certificate_credentials> cred;
    boost::shared_ptr<tls::priority> prio;
  };
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
  impl::context x;

  x.dh.reset(new tls::dh_params(utils::get(socket_data, 2048, "tls", "dh_bits"))); // TODO

  std::string cafile   = utils::get(socket_data, std::string(), "tls", "cafile");
  std::string crlfile  = utils::get(socket_data, std::string(), "tls", "crlfile");
  std::string certfile = utils::get(socket_data, std::string(), "tls", "certfile");
  std::string keyfile  = utils::get(socket_data, std::string(), "tls", "keyfile");

  if (cafile.empty() || crlfile.empty() || certfile.empty() || keyfile.empty())
    throw utils::error("cafile, crlfile, certfile or keyfile not set");

  x.cred.reset(
    new tls::x509_certificate_credentials(
      cafile.c_str(),
      crlfile.c_str(),
      certfile.c_str(),
      keyfile.c_str(),
      *x.dh));

  std::string prio = utils::get(socket_data, std::string("NORMAL"), "tls", "priority");

  x.prio.reset(new tls::priority(prio.c_str()));

  return boost::any(x);
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
  impl::context x = boost::any_cast<impl::context>(scheme_specific);

  http_connection conn(sock.hosts(), addr, servername, log);
  tls::session session(*x.cred, *x.prio, connfd);
  std::auto_ptr<std::streambuf> p(new tls::stream_buffer(session));

  conn.serve(p);
}

// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
