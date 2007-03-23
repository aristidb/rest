// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

/*
== Internal Todo
  * This Code is so horrible!

  * Keep-Alive (check filtering_stream. It closes pushed devices!)
  * complete POST, PUT and HEAD
  * Look for Todos...
  * implement Content-encodings (gzip and co)
  * implement HTTPS
  * remove exceptions
  * clean up!!

difference between content-encoding and transfer-encoding
*/

#include "http-handler.hpp"
#include "rest.hpp"
#include "rest-utils.hpp"

#include <map>
#include <cstdio>
#include <cctype>
#include <string>
#include <boost/tuple/tuple.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include <testsoon.hpp>
#include <iostream> // DEBUG

namespace io = boost::iostreams;
namespace algo = boost::algorithm;

namespace rest {
namespace http {
  namespace {
    struct bad_format { };

    template<class Source, typename Char>
    bool expect(Source &in, Char c) {
      int t = io::get(in);
      if(t == c)
        return true;
      else if(t == Source::traits_type::eof())
        throw bad_format();

      io::putback(in, t);
      return false;
    }

    // checks if `c' is a space or a h-tab (see RFC 2616 chapter 2.2)
    bool isspht(char c) {
      return c == ' ' || c == '\t';
    }

    template<class Source>
    int remove_spaces(Source &in) {
      int c;
      do {
        c = io::get(in);
      } while(isspht(c));
      io::putback(in, c);
      return c;
    }

    typedef std::map<std::string, std::string> header_fields;

    // reads a header field from `in' and adds it to `fields'
    // see RFC 2616 chapter 4.2
    // Warning: Field names are converted to all lower-case!
    template<class Source>
    std::pair<header_fields::iterator, bool> get_header_field
                                     (Source &in, header_fields &fields)
    {
      std::string name;
      int t = 0;
      for(;;) {
        t = io::get(in);
        if(t == '\n' || t == '\r')
          throw bad_format();
        else if(t == Source::traits_type::eof())
          break;
        else if(t == ':') {
          remove_spaces(in);
          break;
        }
        else
          name += std::tolower(t);
      }

      std::string value;
      for(;;) {
        t = io::get(in);
        if(t == '\n' || t == '\r') {
          // Newlines in header fields are allowed when followed
          // by an SP (space or horizontal tab)
          if(t == '\r')
            expect(in, '\n');
          t = io::get(in);
          if(isspht(t)) {
            remove_spaces(in);
            value += ' ';
          }
          else {
            io::putback(in, t);
            break;
          }
        }
        else if(t == Source::traits_type::eof())
          break;
        else
          value += t;
      }

      return fields.insert(std::make_pair(name, value));
    }

    typedef boost::tuple<std::string, std::string, std::string>
              request_line;
    enum { REQUEST_METHOD, REQUEST_URI, REQUEST_HTTP_VERSION };

    template<class Source>
    request_line get_request_line(Source &in) {
      request_line ret;
      int t;
      while( (t = io::get(in)) != ' ') {
        if(t == Source::traits_type::eof())
          throw bad_format();
        ret.get<REQUEST_METHOD>() += t;
      }
      while( (t = io::get(in)) != ' ') {
        if(t == Source::traits_type::eof())
          throw bad_format();
        ret.get<REQUEST_URI>() += t;
      }
      while( (t = io::get(in)) != '\r') {
        if(t == Source::traits_type::eof())
          throw bad_format();
        ret.get<REQUEST_HTTP_VERSION>() += t;
      }
      if(!expect(in, '\n'))
        throw bad_format();
      return ret;
    }
  }

  response http_handler::handle_request(context &global) {
    try {
      std::string method, uri, version;
      boost::tie(method, uri, version) = get_request_line(conn);

      std::cout << method << " " << uri << " " << version << "\n"; // DEBUG

      if(version == "HTTP/1.0") {
        http_1_0_compat = true;
        // TODO HTTP/1.0 compat modus (set connection header to close)
      }
      else if(version != "HTTP/1.1")
        return 505; // HTTP Version not Supported

      header_fields fields;
      for(;;) {
        std::pair<header_fields::iterator, bool> ret =
          get_header_field(conn, fields);

        if(ret.second)
          if(ret.first->first == "accept-encoding") {
            accept_gzip = algo::ifind_first(ret.first->second, "gzip");
            accept_bzip2 = algo::ifind_first(ret.first->second, "bzip2");
          }

        // DEBUG
        if(ret.second)
          std::cerr << ret.first->first << ": "
                    << ret.first->second << "\n";
        else
          std::cerr << "field not added!\n";

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
      else if(method == "HEAD") {
        head_method = true;
        detail::getter_base *getter = responder->x_getter();
        if (!getter || !responder->x_exists(path_id, kw))
          return 404;
        return getter->x_get(path_id, kw);
        // TODO tell get-method not to send entity
      }
      else if(method == "POST" || method == "PUT") {
        // bisschen problematisch mit den keywords
        if(method == "POST") {
          if (!responder->x_exists(path_id, kw) || !responder->x_poster())
            return 404;
        }
        else if(method == "PUT") {
          if (!responder->x_putter())
            return 404;
        }

        // TODO move entity handling to keyword
        header_fields::iterator transfer_encoding =
          fields.find("transfer-encoding");
        bool has_transfer_encoding = transfer_encoding != fields.end();

        io::filtering_istream fin;
        if(has_transfer_encoding) {
          std::cout << "te... " << transfer_encoding->second << std::endl; // DEBUG
          if(algo::iequals(transfer_encoding->second, "chunked"))
            fin.push(utils::chunked_filter());
          else
            return 501;
        }
        
        header_fields::iterator content_length = fields.find("content-length");
        if(content_length == fields.end()) {
          if(!has_transfer_encoding)
            return 411; // Content-length required
        }
        else {
          std::size_t length = boost::lexical_cast<std::size_t>
            (content_length->second);
          fin.push(utils::length_filter(length));
        }
        fin.push(boost::ref(conn), 0, 0);

        fin.set_auto_close(false);//FRESH
        
        header_fields::iterator expect = fields.find("expect");
        if(expect != fields.end()) {
          if(!http_1_0_compat &&
             algo::istarts_with(expect->second, "100-continue"))
            send(100); // Continue
          else
            return 417; // Expectation Failed
        }
        
        //DEBUG
          //std::cout << "reading: " << length << std::endl;
          std::cout << "<<" << fin.rdbuf() << ">>" << std::endl;
        //TODO: an keyword weitergeben

        fin.pop();//FRESH
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
      else if(method == "CONNECT" || method == "OPTIONS")
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

    // Returns a string with the correct formated current Data and Time
    // see RFC 2616 3.3 Date/Time Formats
  }

  void http_handler::send(response const &r) {
    //TODO implement partial-GET, entity data from streams

    io::filtering_ostream out;

    out.set_auto_close(false);

    out.push(boost::ref(conn), 0, 0);
    // Status Line
    if(http_1_0_compat)
      out << "HTTP/1.0 ";
    else
      out << "HTTP/1.1 ";
    std::cout << "Send: " << r.get_code() << " CE:" << (accept_gzip ? "gzip" : (accept_bzip2 ? "bzip2" : "none")) << "\n"; //DEBUG
    out << r.get_code() << " " << r.get_reason() << "\r\n";

    // Header Fields
    out << "Date: " << utils::current_date_time()  << "\r\n";
    out << "Server: " << server_name << "\r\n";
    if(!r.get_type().empty())
      out << "Content-Type: " << r.get_type() << "\r\n";
    if(!r.get_data().empty()) {
      // TODO send length of encoded data if content-encoded! (?)
      out << "Content-Length: " << r.get_data().size() << "\r\n";
      if(accept_gzip)
        out << "Content-Encoding: gzip\r\n";
      else if(accept_bzip2)
        out << "Content-Encoding: bzip2\r\n";
    }
    out << "\r\n";

    // Entity
    if(!head_method) {
      io::filtering_ostream out2;
      out2.set_auto_close(false);
      if(accept_gzip)
        out2.push(io::gzip_compressor());
      else if(accept_bzip2)
        out2.push(io::bzip2_compressor());
      out2.push(boost::ref(out), 0, 0);
      out2 << r.get_data();
      out2.pop();
    }
    out.pop();
  }
}}

namespace {
using namespace rest::http;
using namespace rest::utils;

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
  header_fields fields;
  std::pair<header_fields::iterator, bool> field = get_header_field(x, fields);
  Check(field.second);
  Equals(field.first->first, value[0]);
  Equals(field.first->second, value[1]);
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

  TEST(chunked #1) {
    std::stringstream s1;
    std::size_t n=10;
    std::string s(n, 'x');
    s1 << std::hex << n << "\r\n"
       << s << "\r\n0\r\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), s);
  }

  TEST(chunked #2) {
    std::stringstream s1;
    std::size_t sum = 0;
    for (int i = 0; i < 10; ++i) {
      std::size_t n = 1 + (i % 4);
      sum += n;
      s1 << std::hex << n << "\r\n" << std::string(n, 'x') << "\r\n";
    }
    s1 << "0\r\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), std::string(sum, 'x'));
  }
  
  TEST(chunked #3) {
    std::stringstream s1;
    std::size_t n=10;
    std::string s(n, 'z');
    s1 << std::hex << n << "; very-useless-extension=mega-crap\r\n"
       << s << "\r\n0; useless-extension=crap\r\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), s);
  }

  TEST(chunked invalid) {
    std::stringstream s1;
    s1 << "this is invalid";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), "");
  }

  TEST(chunked invalid: LF only) {
    std::stringstream s1;
    std::size_t n=10;
    std::string s(n, 'x');
    s1 << std::hex << n << "\n"
       << s << "\n0\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), "");
  }
}

}
