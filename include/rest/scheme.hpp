// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SCHEME_HPP
#define REST_SCHEME_HPP

#include "object.hpp"
#include "network.hpp"

namespace rest {

class logger;
class socket_param;

class scheme : public object {
public:
  static std::string const &type_name();

  static bool need_load_standard_objects;
  static void load_standard_objects(object_registry &obj_reg);

  std::string const &type() const { return type_name(); }

  virtual ~scheme() = 0;

  /* schm->serve(log, connfd, sock, addr, servername); */
  virtual void serve(
    logger *log,
    int connfd,
    socket_param const &sock,
    network::address const &addr,
    std::string const &servername) = 0;
};

class http_scheme : public scheme {
public:
  ~http_scheme();

private:
  std::string const &name() const;
  name_list_type const &name_aliases() const;

  void serve(logger*, int, socket_param const&, network::address const&, std::string const&);
};

}

#endif
