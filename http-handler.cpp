// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

/*
== Internal Todo

  * Look for Todos...
  * implement POST, PUT, DELETE
  * implement handling entity data
  * find a way to let the HTTP-Parser and the HTTP-Response (send) method
    talk to each other (e.g. exchange Accept-information)
  * implement Transfer-encodings (chunked, gzip and co)

  * change get_header_field to integrate the fields into a map
  * remove exceptions
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

    bool expect(iostream &in, char c) {
      int t = in.get();
      if(t != c) {
        in.unget();
        return false;
      }
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

      std::cout << method << " " << uri << " " << version << "\n"; // DEBUG

      if(version != "HTTP/1.1")
        return 505; // HTTP Version not Supported

      typedef std::map<std::string, std::string> header_fields;
      header_fields fields;
      for(;;) {
        header_field field = get_header_field(conn);
        fields[field.get<FIELD_NAME>()] = field.get<FIELD_VALUE>();

        std::cout << field.get<FIELD_NAME>() << ": "
                  << field.get<FIELD_VALUE>() << "\n"; // DEBUG

        // TODO do sth with the field
        if(expect(conn, '\r') && expect(conn, '\n'))
          break;
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
          return 404;
        return getter->x_get(path_id, kw);
      }
      else if(method == "POST") {
        detail::poster_base *poster = responder->x_poster();
        if (!poster)
          return 404;

        // Handling Message Entity
        header_fields::iterator expect = fields.find("Expect");
        if(expect != fields.end() &&
           expect->second.compare(0,sizeof("100-continue")-1,
                                  "100-continue") == 0)
          return 100; // Continue

        header_fields::iterator transfer_encoding =
          fields.find("Transfer-Encoding");
        bool has_transfer_encoding = transfer_encoding != fields.end();

        ::std::size_t length;
        header_fields::iterator content_length = fields.find("Content-Length");
        if(content_length == fields.end()) {
          if(!has_transfer_encoding)
            return 411; // Content-length required
        }
        else if(!has_transfer_encoding)
          length =
            ::boost::lexical_cast< ::std::size_t>(content_length->second);

        // TODO check for length limit

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

        // ignore content-le
        if(has_transfer_encoding) {
          if(transfer_encoding->second == "chunked") { // case sensitive?
          }
          else
            ; // implement
          std::cout << "Transfer-Encoding\n";
        }
        else {
          // TODO check if Content-length includes LWS at the end of the header
          std::string s(length, ' ');
          conn.read(&s[0], length);
          std::cout << "Entity: " << s << "\n";
        }
      }
      else if(method == "PUT") {
      }
      else if(method == "DELETE") {
      }
      else if(method == "TRACE") {
#ifndef NO_HTTP_TRACE
        rest::response ret("message/http");
        std::string data = method + " " + uri + " " + version + "\r\n";
        for(header_fields::iterator i = fields.begin();
            i != fields.end();
            ++i)
          data += i->first + ": " + i->second + "\r\n";
        data += "\r\nEntity-Data not included!\r\n"; //TODO include entity data
        ret.set_data(data);
        return ret;
#else
        return 501;
#endif
      }
      else if(method == "HEAD" || method == "CONNECT" || method == "OPTIONS")
        return 501; // Not Supported
      else
        throw bad_format();
    }
    catch(bad_format &e) {
      return 400; // Bad Request
    }
    return 200;
  }

  namespace {
    char const * const server_name = "musikdings.rest";

    // encodes data with chunked tranfer encoding;
    // see RFC 2616 3.6.1 Chunked Transfer Coding
    std::string chunk(iostream &conn, std::string const &data) {
      conn << std::hex << data.length() << "\r\n"
           << data << "\r\n0\r\n";
      return data;
    }

    // Returns a string with the correct formated current Data and Time
    // see RFC 2616 3.3 Date/Time Formats
    std::string current_date_time() {
      // TODO ...
      return "Sat, 10 Mar 2007 01:19:04 GMT";
    }
  }

  void send(response &r, iostream &conn) {
    // Status Line
    conn << "HTTP/1.1 " << ::boost::lexical_cast<std::string>(r.get_code())
         << " " << r.get_reason() << "\r\n";

    // Header Fields
    conn << "Date: " << current_date_time()  << "\r\n";
    conn << "Server: " << server_name << "\r\n";
    if(!r.get_type().empty())
      conn << "Content-Type: " << r.get_type() << "\r\n";
    if(!r.get_data().empty())
      conn << "Transfer-Encoding: chunked\r\n"; // TODO implement gzip and co
    conn << "\r\n";

    // Entity
    if(!r.get_data().empty())
      chunk(conn, r.get_data());
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

    rest::response resp("text/html");
    resp.set_data("<html><head><title>supi</title></head>"
                  "<body><h3>Allles Supi!!</h3><blink>blink</blink>"
                  "<form name=\"input\" action=\"/\""
                  "method=\"post\">"
                  "<input type=\"text\" name=\"user\">"
                  "<input type=\"text\" name=\"bar\">"
                  "<input type=\"submit\" value=\"Submit\">"
                  "</form></body></html>");

    return resp;
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

