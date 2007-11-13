// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SERER_HPP
#define REST_SERVER_HPP

#include <string>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <sys/types.h>
#include <sys/socket.h>

namespace rest {

namespace utils { class property_tree; }
class host;

class server : boost::noncopyable {
public:
  server(utils::property_tree const &config);
  ~server();

  void serve();

  class socket_param {
  public:
    enum socket_type_t { ip4 = AF_INET, ip6 = AF_INET6 };

    socket_param(
        std::string const &service,
        socket_type_t type,
        std::string const &bind,
        long timeout_read,
        long timeout_write);

    socket_param &operator=(socket_param o) {
      o.swap(*this);
      return *this;
    }

    void swap(socket_param &o) {
      p.swap(o.p);
    }

    std::string const &service() const;
    socket_type_t socket_type() const;
    std::string const &bind() const;

    long timeout_read() const;
    long timeout_write() const;

    void add_host(host const &);
    host const *get_host(std::string const &name) const;

  public: // internal
    int fd() const;
    void fd(int f);
    
  private:
    class impl;
    boost::shared_ptr<impl> p;
  };

  typedef std::vector<socket_param>::iterator sockets_iterator;
  typedef std::vector<socket_param>::const_iterator sockets_const_iterator;

  sockets_iterator add_socket(socket_param const &);

  sockets_iterator sockets_begin();
  sockets_iterator sockets_end();
  sockets_const_iterator sockets_begin() const;
  sockets_const_iterator sockets_end() const;

  void sockets_erase(sockets_iterator);
  void sockets_erase(sockets_iterator, sockets_iterator);

  void set_listen_q(int no);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
