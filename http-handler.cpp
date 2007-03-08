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
    struct bad_format { };
    struct not_supported { };

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
	if(t == '\n' || t == '\r') {
	  // Newlines in header fields are allowed when followed
	  // by an SP (space or horizontal tab)
	  if(t == '\r')
	    expect(in, '\n');
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

    typedef ::boost::tuple<std::string, std::string, std::string>
              request_line_t;

    enum { REQUEST_METHOD, REQUEST_URI, REQUEST_HTTP_VERSION };

    request_line_t get_request_line(iostream &in) {
      request_line_t ret;
      int t;
      while( (t = in.get()) != ' ')
	ret.get<REQUEST_METHOD>() += t;
      while( (t = in.get()) != ' ')
	ret.get<REQUEST_URI>() += t;
      while( (t = in.get()) != '\r')
	ret.get<REQUEST_HTTP_VERSION>() += t;
      if(!expect(in, '\n'))
	throw bad_format();
      return ret;
    }
  }

  template<
    unsigned ResponseType,
    typename Path
    >
  void handle_http_request(responder<ResponseType, Path> &r,
			   iostream &conn)
  {
    try {
      std::string method, uri, version;
      boost::tie(method, uri, version) = get_request_line(conn);
      for(;;) {
	
      }

      if(method == "GET") {
	if(r.x_getter())
	  r.x_getter().x_get(uri, keywords());
	else
	  throw not_supported();
      }
      else if(method == "POST") {
      }
      else if(method == "PUT") {
      }
      else if(method == "DELETE") {
      }
      else if(method == "TRACE") {
      }
      else if(method == "HEAD" || method == "CONNECT" || method == "OPTIONS")
	throw not_supported();
      else
	throw bad_format();
    }
    catch(not_supported &e) {
    }
    catch(bad_format &e) {
    }
  }
}}
// for Testing purpose
using namespace rest::http;

int main() {
  std::string method, uri, version;
  boost::tie(method, uri, version) = get_request_line(std::cin);
  std::cout << "Method: " << method << "\nURI: " << uri << "\nVersion: "
	    << version << "\n";
  header_field_t h = get_header_field(std::cin);
  std::cout << "Header Name: " << h.get<FIELD_NAME>()
	    << "\nHeader Value: " << h.get<FIELD_VALUE>() << '\n';
}
