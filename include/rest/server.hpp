// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SERVER_HPP
#define REST_SERVER_HPP

#include "socket_param.hpp"
#include "utils/log.hpp"
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest {

namespace utils { class property_tree; }

class server : boost::noncopyable {
public:
  server(utils::property_tree const &config, utils::logger *log);
  ~server();

  void serve();

  sockets_container::iterator add_socket(socket_param const &);
  sockets_container &sockets();

  void set_listen_q(int no);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
