#include "rest.hpp"

#include <iostream>
#ifdef USE_BOOST_ASIO
#include <boost/asio.hpp>
#endif

namespace rest {
namespace http {
#ifdef USE_BOOST_ASIO
  typedef boost::asio::tcp::iostream iostream;
#else
  // !! Only for testing
  typedef std::istream iostream;
#endif

  namespace {
    bool expect(iostream &in, char c) {
      
    }

    bool expect(iostream &in, char const *s);

    //soft_expect reads any trailing spaces and tries to match the first
    // non-space sequence read
    bool soft_expect(iostream &in, char c);
    bool soft_expect(iostream &in, char const *s);
  }

  void handle_http_connection(responder &r,
			      &conn)
  {
    
  }

}}
