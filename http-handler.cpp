#include "rest.hpp"

#include <cstdio>
#include <cctype>
#include <string>
#include <boost/tuple/tuple.hpp>

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
      return in.get() == c;
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
      } while(std::isspace(t));

      return t == c;
    }

    bool soft_expect(iostream &in, char const *s) {
      char t;
      do {
	t = in.get();
      } while(std::isspace(t));

      std::size_t n = std::strlen(s);
      for(std::size_t i = 0; i < n; ++i, t = in.get())
	if(t != s[i])
	  return false;
      return true;
    }

    // checks if `c' is a space or a h-tab (see RFC 2616 chapter 2.2)
    bool isspht(char c) {
      return c == ' ' || c == '\t';
    }

    int remove_spaces(iostream &in) {
      int c;
      do {
	c = in.get();
      } while(isspht(c));
      in.unget();
      return c;
    }

    typedef ::boost::tuple<std::string, std::string> header_field_t;
    enum { FIELD_NAME, FIELD_VALUE };

    // reads a header field from `in' and returns it (as a tuple name, value)
    // see RFC 2616 chapter 4.2
    header_field_t get_header_field(iostream &in) {
      header_field_t ret;
      std::string *current = &ret.get<FIELD_NAME>();
      int t;
      while(!in.eof() &&
	     in.good()) {
	t = in.get();
	if(t == '\n') { // Newlines in header fields are allowed when followed
                        // by an SP (space or horizontal tab)
	  t = in.get();
	  if(isspht(t)) {
	    remove_spaces(in);
	    *current += ' ';
	  }
	  else {
	    in.unget();
	    break;
	  }
	}
	else if(t == ':' && // Seperates name and value
		current != &ret.get<FIELD_VALUE>())
	{
	  remove_spaces(in);
	  current = &ret.get<FIELD_VALUE>();
	  *current += in.get();
	}
	else
	  *current += t;
      }
      return ret;
    }
  }

  template<
    unsigned ResponseType,
    typename Path
    >
  void handle_http_connection(responder<ResponseType, Path> &r,
			      iostream &conn)
  {
    
  }
}}
// for Testing purpose
using namespace rest::http;

int main() {
  header_field_t h = get_header_field(std::cin);
  std::cout << "Header Name: " << h.get<FIELD_NAME>()
	    << "\nHeader Value: " << h.get<FIELD_VALUE>() << '\n';
}
