#include "rest.hpp"

#include <cstdio>
#include <cctype>

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
      in.get() == c;
    }

    bool expect(iostream &in, char const *s) {
      std::size_t n = std::strlen(s);
      for(std::size_t i = 0; i < n; ++i)
	if(in.get() != s[i])
	  return false;
      return true;
    }

    //soft_expect reads any trailing spaces and tries to match the first
    // non-space sequence read
    bool soft_expect(iostream &in, char c) {
      char t;
      do {
	t = in.get();
      } while(std::isdigit(t));

      return t == c;
    }

    bool soft_expect(iostream &in, char const *s) {
      char t;
      do {
	t = in.get();
      } while(std::isdigit(t));

      std::size_t n = std::strlen(s);
      for(std::size_t i = 0; i < n; ++i)
	if(in.get() != s[i])
	  return false;
      return true;
    }
  }

  void handle_http_connection(responder &r,
			      &conn)
  {
    
  }

}}
