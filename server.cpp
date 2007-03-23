// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/algorithm/string.hpp>

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
namespace io = boost::iostreams;
namespace algo = boost::algorithm;

class server::impl {
public:
  short port;

  typedef
    boost::multi_index_container<
      boost::reference_wrapper<host const>,
      indexed_by<
        hashed_unique<const_mem_fun<host, std::string, &host::get_host> > > >
    cont_t;

  cont_t hosts;

  io::file_descriptor conn;

  typedef std::bitset<4> state_flags;
  enum { // index(!)
    NO_ENTITY,
    HTTP_1_0_COMPAT,
    ACCEPT_GZIP,
    ACCEPT_BZIP2
  };
  state_flags flags;

  static const int LISTENQ = 5; // TODO: configurable

  impl(short port) : port(port) {}

  response handle_request();
  void send(response const &);

  static char const *server_id() { return "Musikdings.rest/0.1"; }
};

server::server(short port) : p(new impl(port)) {}
server::~server() {}

void server::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

void server::serve() {
  int listenfd = ::socket(AF_INET, SOCK_STREAM, 0); // TODO AF_INET6
  if (listenfd == -1)
    throw std::runtime_error("could not start server (socket)"); //sollte errno auswerten!

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

    pid_t pid = ::fork();
    if(pid == -1)
      ; //was ?
    else if(pid == 0) {
      ::close(listenfd);
      {
        for(;;) {
          //http_handler http( ... );
          //response r = http.handle_request( ... ); // <-- hier
          //http.send(r);
        }
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

void server::impl::send(response const &r) {
  //TODO implement partial-GET, entity data from streams

  io::filtering_ostream out;

  out.set_auto_close(false);

  out.push(boost::ref(conn), 0, 0);
  // Status Line
  if (flags.test(HTTP_1_0_COMPAT))
    out << "HTTP/1.0 ";
  else
    out << "HTTP/1.1 ";

  std::cout << "Send: " << r.get_code() << " CE:" << (flags.test(ACCEPT_GZIP) ? "gzip" : (flags.test(ACCEPT_BZIP2) ? "bzip2" : "none")) << "\n"; //DEBUG
  out << r.get_code() << " " << r.get_reason() << "\r\n";

  // Header Fields
  out << "Date: " << utils::current_date_time()  << "\r\n";
  out << "Server: " << server_id() << "\r\n";
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
  if (flags.test(NO_ENTITY)) {
    io::filtering_ostream out2;
    out2.set_auto_close(false);
    if (flags.test(ACCEPT_GZIP))
      out2.push(io::gzip_compressor());
    else if (flags.test(ACCEPT_BZIP2))
      out2.push(io::bzip2_compressor());
    out2.push(boost::ref(out), 0, 0);
    out2 << r.get_data();
    out2.pop();
  }
  out.pop();
}
