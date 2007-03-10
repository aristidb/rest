// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

/*
Internal Todo

* change get_header_field to integrate the fields into a map
* clean up!!

 */

#include "rest.hpp"

#include <map>
#include <cstdio>
#include <cctype>
#include <string>
#include "boost/asio.hpp"
#include <boost/tuple/tuple.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream> // DEBUG

namespace rest {
namespace http {
  typedef ::boost::asio::ip::tcp::iostream iostream;

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

    typedef ::boost::tuple<std::string, std::string> header_field;
    enum { FIELD_NAME, FIELD_VALUE };

    // reads a header field from `in' and returns it (as a tuple name, value)
    // see RFC 2616 chapter 4.2
    header_field get_header_field(iostream &in) {
      header_field ret;
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
              request_line;

    enum { REQUEST_METHOD, REQUEST_URI, REQUEST_HTTP_VERSION };

    request_line get_request_line(iostream &in) {
      request_line ret;
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

  response handle_http_request(context &global, iostream &conn)
  {
    try {
      std::string method, uri, version;
      boost::tie(method, uri, version) = get_request_line(conn);

      std::cout << method << " " << uri << " " << version << "\n";

      if(version != "HTTP/1.1")
        throw not_supported();

      typedef std::map<std::string, std::string> header_fields;
      header_fields fields;
      for(;;) {
        header_field field = get_header_field(conn);
        fields[field.get<FIELD_NAME>()] = field.get<FIELD_VALUE>();

        std::cout << field.get<FIELD_NAME>() << " "
                  << field.get<FIELD_VALUE>() << "\n";

        // TODO do sth with the field
        if(expect(conn, '\r')) {
          if(expect(conn, '\n'))
            break;
          else
            conn.unget();
        }
        else
          conn.unget();
      }

      keywords kw;

      detail::any_path path_id;
      detail::responder_base *responder;
      context *local;
      global.find_responder(uri, path_id, responder, local, kw);

      if (!responder)
        return 404;

      if (method == "GET") {
        detail::getter_base *getter = responder->x_getter();
        if (!getter)
          throw not_supported();
        return getter->x_get(path_id, kw);
      }
      else if(method == "POST") {
        detail::poster_base *poster = responder->x_poster();
        if (!poster)
          throw not_supported();

        header_fields::iterator content_length = fields.find("Content-Length");
        if(content_length == fields.end()) {
          header_fields::iterator expect = fields.find("Expect");
          if(expect == fields.end() ||
             expect->second.compare(0,sizeof("100-continue")-1,
                                    "100-continue") != 0)
            throw bad_format();
          return 100;
        }
        else {
        }

        header_fields::iterator content_type = fields.find("Content-Type");
        if(content_type == fields.end())
          // Set to default value; see RFC 2616 7.2.1 Type
          fields["Content-Type"] = "application/octet-stream";
        else
          // check if the content is multipart
          // see RFC 2046 5.1. Multipart Media Type
          if(content_type->second.compare(0, sizeof("multipart/")-1,
                                          "multipart/") == 0)
            /* ... */;
        
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
      return 404;
    }
    catch(bad_format &e) {
      return 400;
    }
    return 200;
  }

  void send(response &r, iostream &conn) {
    // this is no HTTP/1.1. As usual just for testing
    conn << "HTTP/1.1 " << ::boost::lexical_cast<std::string>(r.get_code())
         << " " << r.get_reason() << "\r\n";

    // Header ...
    conn << "\r\n\r\n" << r.get_data() << "\r\n";
  }
}}


// for Testing purpose
using boost::asio::ip::tcp;
using namespace rest::http;
using namespace rest;

struct tester : rest::responder<rest::GET | rest::PUT | rest::DELETE |
                                rest::POST> {
  rest::response get(std::string const &path, rest::keywords &) {
    std::cout << "GET: " << path << '\n';
    return 200;
  }
  rest::response put(std::string const &path, rest::keywords &) {
    std::cout << "PUT: " << path << '\n';
    return 200;
  }
  rest::response delete_(std::string const &path, rest::keywords &) {
    std::cout << "PUT: " << path << '\n';
    return 200;
  }
  rest::response post(std::string const &path, rest::keywords &) {
    std::cout << "POST: " << path << '\n';
    return 200;
  }
};

int const PORT = 8080;

int main() {
  try {
    boost::asio::io_service io_service;
    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), PORT));
    tester t;
    context c;
    c.bind("/", t);

    for (;;) {
      tcp::iostream stream;
      acceptor.accept(*stream.rdbuf());
      response r = handle_http_request(c, stream);
      send(r, stream);
    }
  }
  catch(std::exception &e) {
    std::cerr << "ERROR: " << e.what() << "\n";
  }
}

