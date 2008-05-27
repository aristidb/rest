#include <rest/tls.hpp>

#include <gcrypt.h>
#include <gnutls/gnutls.h>

#include <cassert>
#include <fstream>

namespace rest { namespace tls {
  gnutls_error::gnutls_error(int ret, std::string const &msg)
    : error(msg + ": " + gnutls_strerror(ret))
  { }

  namespace {
    class gnutls : boost::noncopyable {
      boost::scoped_ptr<dh_params> dh_;
      
      gnutls(unsigned int bits = 2048)
        : dh_(new dh_params(bits))
      {
        if(!gnutls_check_version("2.2.0"))
          throw gnutls_error(0, "Wrong Version");
#ifndef ENABLE_DEV_RANDOM
        gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);
#endif
        gnutls_global_init();
      }

      explicit gnutls(char const *filename)
        : dh_(new dh_params(filename))
      {
        if(!gnutls_check_version("2.2.0"))
          throw gnutls_error(0, "Wrong Version");
#ifndef ENABLE_DEV_RANDOM
        gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);
#endif
        gnutls_global_init();
      }

      ~gnutls() {
        gnutls_global_deinit();
      }
    public:
      dh_params const &dh() const { return *dh_; }

      static gnutls &get(char const *filename = 0x0) {
#ifndef NDEBUG
        static bool initialized = false;
        if(!initialized && !filename)
          throw utils::error("using tls::get without proper initialization!");
#endif
        static gnutls g(filename);
        return g;
      }

      friend void ::rest::tls::reinit_dh_params(unsigned int bits);
      friend void ::rest::tls::reinit_dh_params(char const *filename);
    };
  }

  void init(char const *filename) {
    gnutls::get(filename);
  }

  dh_params const &get_dh_params() {
    return gnutls::get().dh();
  }

  void reinit_dh_params(unsigned int bits) {
    gnutls::get().dh_.reset(new dh_params(bits));
  }

  void reinit_dh_params(char const *filename) {
    gnutls::get().dh_.reset(new dh_params(filename));
  }

  struct dh_params::impl {
    gnutls_dh_params_t dh_params_;
  };

  namespace {
    gnutls_dh_params_t get(dh_params const &dh) {
      return dh.i_get_().dh_params_;
    }
  }

  dh_params::dh_params(char const *filename) {
    load_pkcs3(filename);
  }

  dh_params::dh_params(std::string const &filename) {
    load_pkcs3(filename.c_str());
  }

  void dh_params::load_pkcs3(char const *filename) {
    p.reset(new impl);
    int ret = gnutls_dh_params_init(&p->dh_params_);
    if(ret < 0)
      throw gnutls_error(ret, "initialize dh params");

    std::ifstream in(filename);
    enum { SIZE = 2049 };
    char bitdata[SIZE];
    std::streamsize size = in.readsome(bitdata, SIZE-1);
    if(size < 0)
      throw utils::error("reading dh params from file");
    bitdata[size] = 0;
    
    gnutls_datum params;
    params.data = reinterpret_cast<unsigned char*>(bitdata);
    params.size = size;

    ret = gnutls_dh_params_import_pkcs3(p->dh_params_, &params,
                                        GNUTLS_X509_FMT_PEM);
    if(ret < 0)
      throw gnutls_error(ret, "importing dh params from file");
  }

  dh_params::dh_params(unsigned int bits)
    : p(new impl)
  {
    int ret = gnutls_dh_params_init(&p->dh_params_);
    if(ret < 0)
      throw gnutls_error(ret, "initialize dh params");
    ret = gnutls_dh_params_generate2(p->dh_params_, bits);
    if(ret < 0)
      throw gnutls_error(ret, "generate dh params");
  }

  dh_params::~dh_params() {
    gnutls_dh_params_deinit(p->dh_params_);
  }

  struct x509_certificate_credentials::impl {
    gnutls_certificate_credentials_t credentials;
  };

  namespace {
    gnutls_certificate_credentials_t get(x509_certificate_credentials const &x)
    {
      return x.i_get_().credentials;
    }
  }

  x509_certificate_credentials::x509_certificate_credentials(
    char const *cafile, char const *crlfile, char const *certfile,
    char const *keyfile, dh_params const &dh)
    : p(new impl)
  {
    int ret = gnutls_certificate_allocate_credentials(&p->credentials);
    if(ret < 0)
      throw gnutls_error(ret, "alloc credentials");
    ret = gnutls_certificate_set_x509_trust_file(p->credentials, cafile,
                                                 GNUTLS_X509_FMT_PEM);
    if(ret < 0)
      throw gnutls_error(ret, "trust");
    if(crlfile) {
      ret = gnutls_certificate_set_x509_crl_file(p->credentials, crlfile,
                                                 GNUTLS_X509_FMT_PEM);
      if(ret < 0)
        throw gnutls_error(ret, "crl");
    }
    ret = gnutls_certificate_set_x509_key_file(p->credentials, certfile,
                                               keyfile, GNUTLS_X509_FMT_PEM);
    if(ret < 0)
      throw gnutls_error(ret, "key");
    gnutls_certificate_set_dh_params(p->credentials, get(dh));
  }

  x509_certificate_credentials::~x509_certificate_credentials() {
    gnutls_certificate_free_credentials(p->credentials);
  }

  struct priority::impl {
    gnutls_priority_t priority_cache;
  };

  namespace {
    gnutls_priority_t get(priority const &p) {
      return p.i_get_().priority_cache;
    }
  }

  priority::priority(char const *priorities)
    : p(new impl)
  {
    int ret = gnutls_priority_init(&p->priority_cache, priorities, 0x0);
    if(ret < 0)
      throw gnutls_error(ret, "priority");
  }

  priority::~priority() {
    gnutls_priority_deinit(p->priority_cache);
  }

  struct session::impl {
    gnutls_session_t session_;
    int fd;
  };

  namespace {
    gnutls_session_t get(session const &s) {
      return s.i_get_().session_;
    }
  }

  session::session(x509_certificate_credentials const &cred, 
                   priority const &prio, int fd)
    : p(new impl)
  {
    int ret = gnutls_init(&p->session_, GNUTLS_SERVER);
    if(ret != GNUTLS_E_SUCCESS)
      throw gnutls_error(ret, "session init");

    ret = gnutls_priority_set(p->session_, get(prio));
    if(ret < 0)
      throw gnutls_error(ret, "priority set");

    ret = gnutls_credentials_set(p->session_, GNUTLS_CRD_CERTIFICATE,
                                 get(cred));
    if(ret < 0)
      throw gnutls_error(ret, "credentials set");

    /* request client certificate if any.
     */
    gnutls_certificate_server_set_request(p->session_, GNUTLS_CERT_REQUEST);

#ifdef ENABLE_SSL_COMPATIBILITY
    /* Set maximum compatibility mode. This is only suggested on public
     * webservers that need to trade security for compatibility
     */
    gnutls_session_enable_compatibility_mode(p->session_);
#endif

    // TODO: sollte das hier gemacht werden?
    gnutls_transport_set_ptr(p->session_, (gnutls_transport_ptr_t)fd);
    ret = gnutls_handshake(p->session_);
    if(ret < 0) {
      // TODO deinit
      throw gnutls_error(ret, "handshake");
    }
  }

  session::~session() {
    gnutls_bye(p->session_, GNUTLS_SHUT_WR);
    if(p->fd != -1)
      close(p->fd);
    gnutls_deinit(p->session_);
  }

  std::streamsize device::write(char_type const *buf, std::streamsize n) {
    assert(n >= 0);
    ssize_t res;
    do {
      res = gnutls_record_send(get(session_), buf,
                               n * sizeof(char_type));
    } while(res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN);
    if(res < 0)
      throw gnutls_error(res, "send");
    // TODO Alerts
    return res;
  }

  std::streamsize device::read(char_type *buf, std::streamsize n) {
    assert(n >= 0);
    ssize_t res;
    do {
      res = gnutls_record_recv(get(session_), buf,
                               n * sizeof(char_type));
    } while(res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN);
    // TODO Alerts (zB GNUTLS_E_REHANDSHAKE)
    if(res < 0)
      throw gnutls_error(res, "recv");
    return res;
  }
}}
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
