// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTPS_HPP
#define REST_HTTPS_HPP

#include "scheme.hpp"

namespace rest {

class https_scheme : public scheme {
public:
  ~https_scheme();

private:
  std::string const &name() const;
  void serve(
    logger*,int,socket_param const&,network::address const&,std::string const&);
};

}

#endif
