// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HTTPS_HPP
#define REST_HTTPS_HPP

#include "scheme.hpp"

#include <boost/scoped_ptr.hpp>

namespace rest {

class https_scheme : public scheme {
public:
  https_scheme();
  ~https_scheme();

private:
  class impl;
  boost::scoped_ptr<impl> p;

  std::string const &name() const;
  void serve(
    logger*,int,socket_param const&,network::address const&,std::string const&);
  boost::any create_context(utils::property_tree const &socket_data) const;
};

}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
