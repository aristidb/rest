// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SERVER_HPP
#define REST_SERVER_HPP

#include "socket_param.hpp"
#include "logger.hpp"
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>

#ifndef APPLE
  #include <sys/inotify.h>
#else
  struct inotify_event {};
  #define IN_ALL_EVENTS 0
#endif

namespace rest {

namespace utils { class property_tree; }

class server : boost::noncopyable {
public:
  server(utils::property_tree const &config, logger *log);
  ~server();

  void serve();

  sockets_container::iterator add_socket(socket_param const &);
  sockets_container &sockets();

  void set_listen_q(int no);

  typedef boost::uint32_t inotify_mask_t;
  typedef boost::function<void (inotify_event const &)> watch_callback_t;

  void watch_file(
    std::string const &file_path,
    inotify_mask_t inotify_mask,
    watch_callback_t const &watch_callback);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
