// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

#include <boost/lexical_cast.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include <boost/ref.hpp>

#include <bitset>
#include <map>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <netdb.h>

#include <iostream>//DEBUG

using namespace rest;
using namespace boost::multi_index;
namespace det = rest::detail;
namespace io = boost::iostreams;
namespace algo = boost::algorithm;

#define REST_SERVER_ID "Musikdings.rest/0.1"

typedef
  boost::multi_index_container<
    boost::reference_wrapper<host const>,
    indexed_by<
      hashed_unique<const_mem_fun<host, std::string, &host::get_host> > > >
  hosts_cont_t;

class server::impl {
public:
  short port;

  hosts_cont_t hosts;

  static const int LISTENQ = 5; // TODO: configurable

  impl(short port) : port(port) {}
};

server::server(short port) : p(new impl(port)) {}
server::~server() {}

void server::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

namespace {
  class http_connection {    io::stream_buffer<io::file_descriptor> &conn;
    bool open_;    enum {
      NO_ENTITY,
      HTTP_1_0_COMPAT,
      ACCEPT_GZIP,
      ACCEPT_BZIP2,
      X_NO_FLAG
    };
    typedef std::bitset<X_NO_FLAG> state_flags;    state_flags flags;  public:    http_connection(io::stream_buffer<io::file_descriptor> &conn)      : conn(conn), open_(true) {}

    bool open() const { return open_; }

    void reset_flags() { flags.reset(); }    response handle_request(hosts_cont_t const &hosts);    void send(response const &r);  };
}

void server::serve() {
  int listenfd = ::socket(AF_INET, SOCK_STREAM, 0); // TODO AF_INET6
  if (listenfd == -1)
    throw std::runtime_error("could not start server (socket)"); //sollte errno auswerten!

  int x = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));

  sockaddr_in servaddr;
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // TODO config Frage? jo. für https nur lokal...
  servaddr.sin_port = htons(p->port);
  if (::bind(listenfd, reinterpret_cast<sockaddr const*>(&servaddr), sizeof(servaddr)) == -1)
    throw std::runtime_error("could not start server (bind)");
  if(::listen(listenfd, impl::LISTENQ) == -1)
    throw std::runtime_error("could not start server (listen)");

  for(;;) {
    sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int connfd = ::accept(listenfd, reinterpret_cast<sockaddr *>(&cliaddr), &clilen);
    if(connfd == -1)
      ; //was ?

    std::cout << "%% ACCEPTED" << std::endl;

    pid_t pid = ::fork();
    if(pid == -1)
      ; //was ?
    else if(pid == 0) {
      ::close(listenfd);
      {
        io::stream_buffer<io::file_descriptor> buf(connfd);
        http_connection conn(buf);
        while (conn.open()) {
          conn.reset_flags();
          response r = conn.handle_request(p->hosts);
          conn.send(r);
        }
        std::cout << "%% CLOSING" << std::endl;
        buf.close(); //kommt der hier hin?
      }
      ::exit(0);
    }
    ::close(connfd);
  }
}

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
  std::pair<header_fields::iterator, bool> get_header_field(
      Source &in, header_fields &fields)
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
      } else
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
        } else {
          io::putback(in, t);
          break;
        }
      } else if(t == Source::traits_type::eof())
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

response http_connection::handle_request(hosts_cont_t const &hosts) {
  try {
    std::string method, uri, version;
    boost::tie(method, uri, version) = get_request_line(conn);

    std::cout << method << " " << uri << " " << version << "\n"; // DEBUG

    if (version == "HTTP/1.0") {
      flags.set(HTTP_1_0_COMPAT);
      open_ = false;
    } else if(version != "HTTP/1.1")
      return 505; // HTTP Version not Supported

    header_fields fields;
    for (;;) {
      std::pair<header_fields::iterator, bool> ret =
        get_header_field(conn, fields);

      if (ret.second)
        if (ret.first->first == "accept-encoding") {
          flags.set(ACCEPT_GZIP, algo::ifind_first(ret.first->second, "gzip"));
          flags.set(ACCEPT_BZIP2, algo::ifind_first(ret.first->second, "bzip2"));
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

    header_fields::iterator host_header = fields.find("host");
    if(host_header == fields.end())
      return 400;
    std::string::const_iterator begin = host_header->second.begin();
    std::string::const_iterator end = host_header->second.end();
    std::string::const_iterator delim = std::find(begin, end, ':');
    std::string the_host(begin, delim);

    std::cout << "THE HOST: " << the_host << std::endl;

    hosts_cont_t::const_iterator it = hosts.find(the_host);
    // TODO: strip subdomains until found
    if (it == hosts.end())
      return 404; // or whatever to indicate that this is no good host

    host const &host = *it;
    context &global = host.get_context();

    if(!flags.test(HTTP_1_0_COMPAT)) {
      header_fields::iterator connect_header = fields.find("connect");
      if(connect_header != fields.end() && connect_header->second == "close")
        open_ = false;
    }

    keywords kw;

    det::any_path path_id;
    det::responder_base *responder;
    context *local;
    global.find_responder(uri, path_id, responder, local, kw);

    if (!responder)
      return 404;

    if (method == "GET") {
      det::getter_base *getter = responder->x_getter();
      if (!getter || !responder->x_exists(path_id, kw))
        return 404;
      return getter->x_get(path_id, kw);
    }
    else if(method == "HEAD") {
      flags.set(NO_ENTITY);
      det::getter_base *getter = responder->x_getter();
      if (!getter || !responder->x_exists(path_id, kw))
        return 404;
      return getter->x_get(path_id, kw);
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
        std::size_t length =
            boost::lexical_cast<std::size_t>(content_length->second);
        fin.push(utils::length_filter(length));
      }
      fin.push(boost::ref(conn), 0, 0);

      fin.set_auto_close(false);//FRESH

      header_fields::iterator expect = fields.find("expect");
      if (expect != fields.end()) {
        if (!flags.test(HTTP_1_0_COMPAT) &&
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
    else if (method == "DELETE") {
      det::deleter_base *deleter = responder->x_deleter();
      if (!deleter || !responder->x_exists(path_id, kw))
        return 404;
      return deleter->x_delete(path_id, kw);
    }
    else if (method == "TRACE") {
#ifndef NO_HTTP_TRACE
      rest::response ret("message/http");
      std::string data = method + " " + uri + " " + version + "\r\n";
      for (header_fields::iterator i = fields.begin();
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
    else if (method == "CONNECT" || method == "OPTIONS")
      return 501; // Not Supported
    else
      throw bad_format();
  } catch (bad_format &e) {
    return 400; // Bad Request
  }
  return 200;
}

void http_connection::send(response const &r) {
  //TODO implement partial-GET, entity data from streams

  io::filtering_ostream out(boost::ref(conn), 0, 0);
  out.set_auto_close(false);

  // Status Line
  if (flags.test(HTTP_1_0_COMPAT))
    out << "HTTP/1.0 ";
  else
    out << "HTTP/1.1 ";

  std::cout << "Send: " << r.get_code() << " CE:" << (flags.test(ACCEPT_GZIP) ? "gzip" : (flags.test(ACCEPT_BZIP2) ? "bzip2" : "none")) << "\n"; //DEBUG
  out << r.get_code() << " " << r.get_reason() << "\r\n";

  // Header Fields
  out << "Date: " << utils::current_date_time()  << "\r\n";
  out << "Server: " << REST_SERVER_ID << "\r\n";
  if (!r.get_type().empty())
    out << "Content-Type: " << r.get_type() << "\r\n";
  if (!r.get_data().empty()) {
    // TODO send length of encoded data if content-encoded! (?)
    out << "Content-Length: " << r.get_data().size() << "\r\n";
    if (flags.test(ACCEPT_GZIP))
      out << "Content-Encoding: gzip\r\n";
    else if (flags.test(ACCEPT_BZIP2))
      out << "Content-Encoding: bzip2\r\n";
  }
  out << "\r\n";

  // Entity
  if (!r.get_data().empty() && !flags.test(NO_ENTITY)) {
    io::filtering_ostream out2;
    out2.set_auto_close(false);
    if (flags.test(ACCEPT_GZIP))
      out2.push(io::gzip_compressor());
    else if (flags.test(ACCEPT_BZIP2))
      out2.push(io::bzip2_compressor());
    out2.push(boost::ref(out), 0, 0);
    //out2.push(boost::ref(std::cout), 0, 0); DEBUG
    out2 << r.get_data();
    out2.pop();
  }
  out.pop();
}
