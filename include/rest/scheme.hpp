// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SCHEME_HPP
#define REST_SCHEME_HPP

#include "object.hpp"
#include "network.hpp"
#include <boost/any.hpp>

namespace rest {

class logger;
class socket_param;

namespace utils { class property_tree; }

class scheme : public object {
public:
  static std::string const &type_name();

  static bool need_load_standard_objects;
  static void load_standard_objects(object_registry &obj_reg);

  std::string const &type() const { return type_name(); }

  virtual ~scheme() = 0;

  virtual void serve(
    logger *log,
    int connfd,
    socket_param const &sock,
    network::address const &addr,
    std::string const &servername) = 0;

  virtual boost::any create_context(utils::property_tree const &socket_data) const;
};

class http_scheme : public scheme {
public:
  ~http_scheme();

private:
  std::string const &name() const;

  void serve(
    logger*,int,socket_param const&,network::address const&,std::string const&);
};

}

#endif
