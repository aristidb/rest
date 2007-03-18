// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

/*
== Internal Todo
  * This Code is so horrible!

  * Look for Todos...
  * implement POST, PUT
  * implement handling entity data
  * find a way to let the HTTP-Parser and the HTTP-Response (send) method
    talk to each other (e.g. exchange Accept-information)
  * implement Transfer-encodings (chunked, gzip and co)

  * change get_header_field to integrate the fields into a map
  * remove exceptions
  * clean up!!
*/

#include "http-handler.hpp"
#include "rest.hpp"

#include <map>
#include <cstdio>
#include <cctype>
#include <string>
#include <boost/tuple/tuple.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include <testsoon.hpp>
#include <iostream> // DEBUG
#include <sstream>

namespace io = boost::iostreams;

namespace rest {
namespace http {
  typedef std::iostream iostream;
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
        else if(t == iostream::traits_type::eof())
          break;
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

  boost::tuple<bool,int> hex2int(int ascii) {
    if(std::isdigit(ascii))
      return boost::make_tuple(true, ascii - '0');
    else if(ascii >= 'a' && ascii <= 'f')
      return boost::make_tuple(true, ascii - 'a' + 0xa);
    else if(ascii >= 'A' && ascii <= 'F')
      return boost::make_tuple(true, ascii - 'A' + 0xa);
    else
      return boost::make_tuple(false, 0);
  }

  class chunked_filter : public io::multichar_input_filter {
  public:
    chunked_filter() : pending(0) {}
  
    template<typename Source>
    std::streamsize read(Source &source, char *outbuf, std::streamsize n) {
      if (pending == 0) {
        // read header and set pending
        int c;
        c = io::get(source);
        if(c == EOF)
          return -1;
        else if(c == '\r') {
          c = io::get(source);
          if(c != '\n')
           return -1;
          c = io::get(source);
          if(c == EOF)
            return -1;
        }

        for(;;) {
          boost::tuple<bool, int> value = hex2int(c);
          if(value.get<0>()) {
            pending *= 0x10;
            pending += value.get<1>();
          }
          else
            break;
          c = io::get(source);
          if(c == EOF)
            return -1;
        }
        bool cr = false;
        for(;;) {
          if(c == '\r')
            cr = true;
          else if(cr && c == '\n')
            break;
          else if(c == EOF)
            return -1;
          c = io::get(source);
        }
      }
      std::streamsize c;
      if (n <= pending)
        c = io::read(source, outbuf, n);
      else
        c = io::read(source, outbuf, pending);
      if (c == -1)
        return -1;
      pending -= c;
      return c;
    }

  private:
    std::streamsize pending;
  };

  class length_filter : public io::multichar_input_filter {
  public:
    length_filter(std::streamsize length) : length(length) {}

    template<typename Source>
    std::streamsize read(Source &source, char *outbuf, std::streamsize n) {
      std::streamsize c;
      if (n <= length)
        c = io::read(source, outbuf, n);
      else
        c = io::read(source, outbuf, length);
      if (c == -1)
        return -1;
      length -= c;
      return c;
    }

  private:
    std::streamsize length;
  };

  response http_handler::handle_request(context &global)
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
        if (!getter || !responder->x_exists(path_id, kw))
          return 404;
        return getter->x_get(path_id, kw);
      }
      else if(method == "POST") {
        detail::poster_base *poster = responder->x_poster();
        if (!poster || !responder->x_exists(path_id, kw))
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
        else
          length =
            ::boost::lexical_cast< ::std::size_t>(content_length->second);

        // TODO check for length limit
/*
        bool is_multipart = false;
        header_fields::iterator content_type = fields.find("Content-Type");
        if(content_type == fields.end())
          // Set to default value; see RFC 2616 7.2.1 Type
          fields["Content-Type"] = "application/octet-stream";
        else
          // check if the content is multipart
          // see RFC 2046 5.1. Multipart Media Type
          if(content_type->second.compare(0, sizeof("multipart/")-1,
                                          "multipart/") == 0)
          {
            is_multipart = true;
            std::string const &type = content_type->second;
            for(;;) {
              std::size_t pos = type.find(';', 0);
              if(pos == std::string::npos)
                throw bad_format();
              ++pos;
              while(type[pos] == ' ') // ignore empty spaces
                ++pos;
              if(type.compare(pos, sizeof("boundary=")-1,
                              "boundary=") == 0)
                {
                  pos += sizeof("boundary=");
                  std::size_t end = pos;
                  if(type[pos] == '"') {
                    ++pos; ++end;
                    // TODO is there a way to escape " in MIME-Flags?
                    while(type[end] != '"' && type[end] != '\r')
                      ++end;
                  }
                  else
                    while(type[end] != ';' && !std::isspace(type[end]))
                      ++end;

                  fin.push(boundary_filter(type.substr(pos, end))); // ist das wirklich aufgabe dieser funktion?
                  // ist mir suspekt, dass diese funktion genau den ersten boundary rausholt aber sonst keinen, oder?
                  break;
                }
            }
          }
*/
        io::filtering_istream fin;
        if(has_transfer_encoding) {
          if(transfer_encoding->second == "chunked") // case sensitive?
            fin.push(chunked_filter());
          else
            ; // TODO implement
          std::cout << "Transfer-Encoding\n"; // DEBUG 
        }
        fin.push(*conn.rdbuf()); 
        
        //if(!is_multipart) {
          // TODO check if Content-length includes LWS at the end of the header
          std::string s(length, ' ');
          fin.read(&s[0], length); // <- hier isses nich initialisiert. manchmal zumindest
          // TODO: check fin
          std::cout << "Entity: " << s << "\n"; // DEBUG
          
          // und was nu mit den Daten?
          // auf cout ausgeben und testen.
          // bzw.: an keywords weiterreichen
        //}
      }
      else if(method == "PUT") {
      }
      else if(method == "DELETE") {
        detail::deleter_base *deleter = responder->x_deleter();
        if (!deleter || !responder->x_exists(path_id, kw))
          return 404;
        return deleter->x_delete(path_id, kw);
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

  void http_handler::send(response &r) {
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

namespace {
using namespace rest::http;

XTEST((values, (std::string)("ab")("\r\n"))) {
  std::stringstream x(value);
  Equals(expect(x, value[0]), true);
  Equals(x.get(), value[1]);
}

XTEST((values, (std::string)("ab")("\r\n"))) {
  std::stringstream x(value);
  Not_equals(value[0], value[1]);
  Equals(expect(x, value[1]), false);
  Equals(x.get(), value[0]);
}

XTEST((values, (char)(' ')('\t'))) {
  Check(isspht(value));
}

XTEST((values, (char)('\n')('\v')('\a')('a'))) {
  Check(!isspht(value));
}

TEST() {
  char const *value[] = { "foo", "bar, kotz=\"haHA;\"" };

  std::string header(value[0]);
  header += ":         ";
  header += value[1];
  std::stringstream x(header);
  header_field field = get_header_field(x);
  Equals(field.get<FIELD_NAME>(), value[0]);
  Equals(field.get<FIELD_VALUE>(), value[1]);
}

XTEST((values, (std::string)("   x")("\t\ny")(" z "))) {
  std::stringstream x(value);
  int r = remove_spaces(x);
  int t = x.get();
  Equals(r, t);
  Check(!isspht(t));
}

TEST() {
  std::string line = "GET /foo/?bar&k=kk HTTP/1.1\r\n";
  std::stringstream x(line);
  request_line req = get_request_line(x);
  Equals(req.get<REQUEST_METHOD>(), "GET");
  Equals(req.get<REQUEST_URI>(), "/foo/?bar&k=kk");
  Equals(req.get<REQUEST_HTTP_VERSION>(), "HTTP/1.1");
}

TEST_GROUP(aux) {
  XTEST((values, (char)('a')('1')('F'))) {
    std::stringstream s;
    s << value;
    int x;
    s >> std::hex >> x;
    boost::tuple<bool, int> t = hex2int(value);
    Check(t.get<0>());
    Equals(t.get<1>(), x);
  }
}

TEST_GROUP(filters) {
  TEST(length #1) {
    std::stringstream s1;
    s1 << std::string(133, 'x');

    io::filtering_istream fs;
    fs.push(length_filter(100));
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), std::string(100, 'x'));
  }

  TEST(length #2) {
    std::stringstream s1;
    s1 << std::string(40, 'x');

    io::filtering_istream fs;
    fs.push(length_filter(100));
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), std::string(40, 'x'));
  }
}

}
