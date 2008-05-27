#ifndef REST_TLS_HPP
#define REST_TLS_HPP

#include "rest/utils/exceptions.hpp"
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest { namespace tls {
  struct gnutls_error : utils::error {
    explicit gnutls_error(int ret, std::string const &msg = "");
  };

  void init(unsigned int bits = 2048);

  class dh_params : boost::noncopyable {
    struct impl;
    boost::scoped_ptr<impl> p;

    void load_pkcs3(char const *filename);
  public:
    explicit dh_params(unsigned int bits = 2048);
    explicit dh_params(char const *filename);
    explicit dh_params(std::string const &filename);
    ~dh_params();

    impl const &i_get_() const { return *p.get(); } // internal!
  };

  dh_params const &get_dh_params();
  void reinit_dh_params(unsigned int bits = 2048);
  void reinit_dh_params(char const *filename);

  class x509_certificate_credentials : boost::noncopyable {
    struct impl;
    boost::scoped_ptr<impl> p;
  public:
    x509_certificate_credentials(char const *cafile, char const *crlfile,
                                 char const *certfile, char const *keyfile,
                                 dh_params const &dh);
    ~x509_certificate_credentials();

    impl const &i_get_() const { return *p.get(); } // internal!
  };

  class priority : boost::noncopyable {
    struct impl;
    boost::scoped_ptr<impl> p;  
  public:
    explicit priority(char const *priorities = "NORMAL");
    ~priority();

    impl const &i_get_() const { return *p.get(); } // internal!
  };

  class session : boost::noncopyable {
    struct impl;
    boost::scoped_ptr<impl> p;
  public:
    session(x509_certificate_credentials const &cred, priority const &prio,
            int fd);
    //explicit session(gnutls_session_t session_, int fd = -1);
    ~session();

    impl const &i_get_() const { return *p.get(); } // internal!
  };

  struct device {
    typedef boost::iostreams::bidirectional_device_tag category;
    typedef char char_type;

    std::streamsize write(char_type const *buf, std::streamsize n);
    std::streamsize read(char_type *buf, std::streamsize n);

    explicit device(session const &session_)
      : session_(session_)
    { }
  private:
    session const &session_;
  };

  typedef boost::iostreams::stream<device> stream;
  typedef boost::iostreams::stream_buffer<device> stream_buffer;
}}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
