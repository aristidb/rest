#include <cassert>
#include <stdexcept>

#include <gcrypt.h>
#include <gnutls/gnutls.h>

#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/noncopyable.hpp>

#ifndef NO_CIRCULAR_BUFFER
#include <boost/circular_buffer.hpp>
#else
#include <vector>
#endif

#include <string>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace tls {
  struct gnutls_error : std::runtime_error {
    explicit gnutls_error(int ret, std::string const &msg = "")
      : std::runtime_error(msg + ": " + gnutls_strerror(ret))
    { }
  };

  class gnutls : boost::noncopyable {
    gnutls() {
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
    friend void init();
  };

  void init() {
    static gnutls g;
  }

  class dh_params : boost::noncopyable {
    gnutls_dh_params_t dh_params_;
  public:
    explicit dh_params(unsigned int bits = 2048) {
      gnutls_dh_params_init(&dh_params_);
      gnutls_dh_params_generate2(dh_params_, bits);
    }

    ~dh_params() {
      gnutls_dh_params_deinit(dh_params_);
    }

    gnutls_dh_params_t get() const { return dh_params_; }
  };

  class x509_certificate_credentials : boost::noncopyable {
    gnutls_certificate_credentials_t credentials;
  public:
    x509_certificate_credentials(char const *cafile, char const *crlfile,
                               char const *certfile, char const *keyfile,
                               dh_params const &dh)
    {
      int ret = gnutls_certificate_allocate_credentials(&credentials);
      if(ret < 0)
        throw gnutls_error(ret, "alloc credentials");
      ret = gnutls_certificate_set_x509_trust_file(credentials, cafile,
                                                       GNUTLS_X509_FMT_PEM);
      if(ret < 0)
        throw gnutls_error(ret, "trust");
      if(crlfile) {
        ret = gnutls_certificate_set_x509_crl_file(credentials, crlfile,
                                                   GNUTLS_X509_FMT_PEM);
        if(ret < 0)
          throw gnutls_error(ret, "crl");
      }
      ret = gnutls_certificate_set_x509_key_file(credentials, certfile, keyfile,
                                                 GNUTLS_X509_FMT_PEM);
      if(ret < 0)
        throw gnutls_error(ret, "key");
      gnutls_certificate_set_dh_params(credentials, dh.get());
    }

    ~x509_certificate_credentials() {
      gnutls_certificate_free_credentials(credentials);
    }

    gnutls_certificate_credentials_t get() const { return credentials; }
  };

  class priority : boost::noncopyable {
    gnutls_priority_t priority_cache;
  public:
    explicit priority(char const *priorities = "NORMAL") {
      int ret = gnutls_priority_init(&priority_cache, priorities, 0x0);
      if(ret < 0)
        throw gnutls_error(ret, "priority");
    }

    ~priority() {
      gnutls_priority_deinit(priority_cache);
    }

    gnutls_priority_t get() const { return priority_cache; }
  };

  namespace {
    class session_db : boost::noncopyable {

      static std::size_t const cache_size = 50;

    private:
      session_db() : cb(cache_size) { }
      ~session_db() { }

      enum {
        MAX_SESSION_ID_SIZE = 32,
        MAX_SESSION_DATA_SIZE = 512
      };
      struct entry {
        entry(gnutls_datum_t const &key, gnutls_datum_t const &data)
          : key(key), data(data)
        { }

        gnutls_datum_t key;
        gnutls_datum_t data;
      };
#ifndef NO_CIRCULAR_BUFFER
      typedef boost::circular_buffer<entry> buffer_t;
#else
      typedef std::vector<entry> buffer_t;
#endif
      buffer_t cb;

      static session_db &get() {
        static session_db db;
        return db;
      }

      struct datum_finder {
        gnutls_datum_t const &key;
        datum_finder(gnutls_datum_t const &key) : key(key) { }

        bool operator()(entry const &e) {
          return
            e.key.size == key.size &&
            std::memcmp(e.key.data, key.data, key.size) == 0;
        }
      };

      buffer_t::iterator find(gnutls_datum_t const &key) {
        return std::find_if(cb.begin(), cb.end(), datum_finder(key));
      }
    public:
      static gnutls_datum_t fetch(void*, gnutls_datum_t key) {
        session_db &db = session_db::get();
        buffer_t::iterator i = db.find(key);
        if(i != db.cb.end()) {
          return i->data;
        }
        gnutls_datum_t nil;
        std::memset(&nil, 0, sizeof(nil));
        return nil;
      }
      static int delete_(void*, gnutls_datum_t key) {
        session_db &db = session_db::get();
        buffer_t::iterator i = db.find(key);
        if(i != db.cb.end()) {
          db.cb.erase(i);
          return 0;
        }
        return -1;
      }
      static int store(void*, gnutls_datum_t key, gnutls_datum_t data) {
        session_db &db = session_db::get();
        db.cb.push_back(entry(key, data));
        return 0;
      }
    };
  }

  class session : boost::noncopyable {
    gnutls_session_t session_;
    int fd;
  public:
    session(x509_certificate_credentials const &cred, priority const &prio,
            int fd)
    {
      int ret = gnutls_init(&session_, GNUTLS_SERVER);
      if(ret != GNUTLS_E_SUCCESS)
        throw gnutls_error(ret, "session init");

      ret = gnutls_priority_set(session_, prio.get());
      if(ret < 0)
        throw gnutls_error(ret, "priority set");

      ret = gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE,
                                   cred.get());
      if(ret < 0)
        throw gnutls_error(ret, "credentials set");

      /* request client certificate if any.
       */
      gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUEST);

#ifdef ENABLE_SSL_COMPATIBILITY
      /* Set maximum compatibility mode. This is only suggested on public
       * webservers that need to trade security for compatibility
       */
      gnutls_session_enable_compatibility_mode(session_);
#endif

#ifndef NO_SESSION_CACHE
      gnutls_db_set_retrieve_function(session_, session_db::fetch);
      gnutls_db_set_remove_function(session_, session_db::delete_);
      gnutls_db_set_store_function(session_, session_db::store);
      gnutls_db_set_ptr(session_, 0x0);
#endif

      // TODO: sollte das hier gemacht werden?
      gnutls_transport_set_ptr(session_, (gnutls_transport_ptr_t)fd);
      ret = gnutls_handshake(session_);
      if(ret < 0) {
        // TODO deinit
        throw gnutls_error(ret, "handshake");
      }
    }

    explicit session(gnutls_session_t session_, int fd = -1)
      : session_(session_), fd(fd)
    { }

    ~session() {
      gnutls_bye(session_, GNUTLS_SHUT_WR);
      if(fd != -1)
        close(fd);
      gnutls_deinit(session_);
    }

    gnutls_session_t get() const { return session_; }
  };

  template<typename Char>
  struct basic_device {
    typedef boost::iostreams::bidirectional_device_tag category;
    typedef Char char_type;

    std::streamsize write(char_type const *buf, std::streamsize n) {
      assert(n >= 0);
      ssize_t res;
      do {
        res = gnutls_record_send(session_.get(), buf,
                                 n * sizeof(char_type));
      } while(res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN);
      if(res < 0)
        throw gnutls_error(res, "send");
      // TODO Alerts
      return res;
    }

    std::streamsize read(char_type *buf, std::streamsize n) {
      assert(n >= 0);
      ssize_t res;
      do {
        res = gnutls_record_recv(session_.get(), buf,
                                 n * sizeof(char_type));
      } while(res == GNUTLS_E_INTERRUPTED || res == GNUTLS_E_AGAIN);
      // TODO Alerts (zB GNUTLS_E_REHANDSHAKE)
      if(res < 0)
        throw gnutls_error(res, "recv");
      return res;
    }

    explicit basic_device(session const &session_)
      : session_(session_)
    { }
  private:
    session const &session_;
  };

  typedef basic_device<char> device;
  typedef boost::iostreams::stream<device> stream;
  typedef boost::iostreams::stream_buffer<device> stream_buffer;
}

namespace {
  char const * const keyfile = "x509-server-key.pem";
  char const * const certfile = "x509-server.pem";
  char const * const cafile = "x509-ca.pem";
  char const * const crlfile = 0x0;
  char const * const socket_file = "local.socket";

  void server(int fd)
    try {
      tls::init();

      tls::dh_params dh(768);
      std::cerr << "Cert\n";
      tls::x509_certificate_credentials x509_cert(cafile, crlfile, certfile,
                                                  keyfile, dh);
      tls::priority priority;

      std::cerr << "Awaiting Connections\n";
      for(;;) {
        sockaddr_un remote;
        socklen_t len = sizeof(remote);
        int cd = ::accept(fd, (sockaddr*)&remote, &len);
        if(cd == -1) {
          close(fd);
          perror("accept");
          return;
        }

        tls::session session(x509_cert, priority, cd);
        tls::stream out(session);
        out << "Hallo Welt!\n";
      }
    }
    catch(std::exception const &e) {
      std::cerr << "SERVER: " << e.what() << '\n';
    }

  void client()
    try {
      sleep(3);
      std::cerr << "start client\n";

      tls::init();

      gnutls_certificate_credentials_t cred;
      int ret = gnutls_certificate_allocate_credentials(&cred);
      if(ret < 0)
        throw tls::gnutls_error(ret, "anon alloc");
      ret = gnutls_certificate_set_x509_trust_file(cred, cafile,
                                                   GNUTLS_X509_FMT_PEM);
      if(ret < 0)
        throw tls::gnutls_error(ret, "trust");

      gnutls_session_t session;
      ret = gnutls_init(&session, GNUTLS_CLIENT);
      if(ret < 0)
        throw tls::gnutls_error(ret, "init session");
      ret = gnutls_priority_set_direct(session, "NORMAL", 0x0);
      if(ret < 0)
        throw tls::gnutls_error(ret, "priority direct");
      gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cred);

      int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
      if(fd == -1) {
        perror("Csocket");
        return;
      }

      sockaddr_un remote;
      remote.sun_family = AF_UNIX;
      strcpy(remote.sun_path, socket_file);
      if(connect(fd, (sockaddr*)&remote, sizeof(remote)) == -1) {
        perror("Cconnect");
        return;
      }

      gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t)fd);

      ret = gnutls_handshake(session);
      if(ret < 0)
        throw tls::gnutls_error(ret, "handshake");

      tls::session s(session, fd);
      tls::stream in(s);

      std::string str;
      std::getline(in, str);
      std::cerr << "GOT: '" << str << "'\n";
      assert(str == "Hallo Welt!");
      std::cerr << "HAPPY!\n";
    }
    catch(std::exception &e) {
      std::cerr << "CLIENT: " << e.what() << '\n';
    }
}

int main() {
  std::signal(SIGPIPE, SIG_IGN);

  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd == -1) {
    perror("socket");
    return 1;
  }
  sockaddr_un local;
  local.sun_family = AF_UNIX;
  std::strcpy(local.sun_path, socket_file);
  ::unlink(local.sun_path);
  if(::bind(fd, (sockaddr*)&local, sizeof(local)) == -1) {
    perror("bind");
    return 1;
  }

  if(::listen(fd, 5) == -1) {
    perror("listen");
    return 1;
  }

  int status = 0;
  switch(::fork()) {
  case -1:
    std::perror("pipe");
    return 1;
  case 0:
    ::close(fd);
    client();
    break;
  default:
    server(fd);
    ::wait(&status);
  }
}
