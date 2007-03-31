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
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include <boost/ref.hpp>

#include <cstring>
#include <bitset>
#include <map>
#include <limits>

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <iostream>//DEBUG

using namespace rest;
using namespace boost::multi_index;
namespace det = rest::detail;
namespace io = boost::iostreams;
namespace algo = boost::algorithm;

/*
 * Big TODO:
 *
 * - see below (for more)
 */

#define REST_SERVER_ID "Musikdings.rest/0.1"

typedef
  boost::multi_index_container<
    boost::reference_wrapper<host const>,
    indexed_by<
      hashed_unique<const_mem_fun<host, std::string, &host::get_host> > > >
  hosts_cont_t;

namespace {
  struct remote_close {};
}

class server::impl {
public:
  short port;

  hosts_cont_t hosts;

  static const int LISTENQ = 5; // TODO: configurable

  impl(short port) : port(port) {}

  static void sigchld_handler(int) {
    while (::waitpid(-1, 0, WNOHANG) > 0)
      ;
  }
};

server::server(short port) : p(new impl(port)) {}
server::~server() {}

void server::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

typedef io::stream_buffer<utils::socket_device> connection_streambuf;

namespace {
  class http_connection {
    connection_streambuf &conn;
    bool open_;
    enum {
      NO_ENTITY,
      HTTP_1_0_COMPAT,
      ACCEPT_GZIP,
      ACCEPT_BZIP2,
      X_NO_FLAG
    };
    typedef std::bitset<X_NO_FLAG> state_flags;
    state_flags flags;

  public:
    http_connection(connection_streambuf &conn)
      : conn(conn), open_(true) {}

    bool open() const { return open_; }

    typedef std::map<std::string, std::string> header_fields;
    header_fields read_headers();

    void reset_flags() { flags.reset(); }
    response handle_request(hosts_cont_t const &hosts);
    int handle_entity(keywords &kw, header_fields &fields);

    void send(response const &r, bool entity);
    void send(response const &r) {
      send(r, !flags.test(NO_ENTITY));
    }


    static
    hosts_cont_t::const_iterator find_host(
      header_fields const &fields, hosts_cont_t const &hosts);
    
    template<class Source>
    static
    std::pair<header_fields::iterator, bool> 
    get_header_field(Source &in, header_fields &fields);
  };
}

void server::serve() {
  void (*oldhandler)(int) = ::signal(SIGCHLD, &impl::sigchld_handler);

  int listenfd = ::socket(AF_INET, SOCK_STREAM, 0); // TODO AF_INET6
  if (listenfd == -1)
    throw std::runtime_error("could not start server (socket)"); //sollte errno auswerten!

  int x = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));

  sockaddr_in servaddr;
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // TODO config Frage? jo. für https nur lokal...
  servaddr.sin_port = htons(p->port);
  if (::bind(listenfd, (sockaddr *) &servaddr, sizeof(servaddr)) == -1)
    throw std::runtime_error("could not start server (bind)");
  if(::listen(listenfd, impl::LISTENQ) == -1)
    throw std::runtime_error("could not start server (listen)");

  for(;;) {
    sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int connfd = ::accept(listenfd, (sockaddr *) &cliaddr, &clilen);
    if(connfd == -1) {
      REST_LOG_ERRNO(utils::CRITICAL, "accept failed");
      continue;
    }

    std::cout << "%% ACCEPTED" << std::endl; // DEBUG

    pid_t pid = ::fork();
    if(pid == -1) {
      REST_LOG_ERRNO(utils::CRITICAL, "fork failed");
    }
    else if(pid == 0) {
      ::close(listenfd);
      int status = 0;
      {
        try {
          connection_streambuf buf(connfd, 10);
          http_connection conn(buf);
          try {
            while (conn.open()) {
              conn.reset_flags();
              response r = conn.handle_request(p->hosts);
              conn.send(r);
            }
          }
          catch(remote_close&) {
            std::cout << "%% remote" << std::endl; // DEBUG
          }
          std::cout << "%% CLOSING" << std::endl; // DEBUG
        }
        catch(std::exception &e) {
          REST_LOG_E(utils::CRITICAL,
                     "ERROR: unexpected exception `" << e.what() << "'");
          status = 1;
        }
        catch(...) {
          REST_LOG_E(utils::CRITICAL,
                     "ERROR: unexpected exception (unkown type)");
          status = 1;
        }
      }
      ::exit(status);
    }
    else
      ::close(connfd);
  }

  ::signal(SIGCHLD, oldhandler);
}

namespace {
  struct bad_format { };

  template<class Source, typename Char>
  bool expect(Source &in, Char c) {
    int t = io::get(in);
    if(t == c)
      return true;
    else if(t == Source::traits_type::eof())
      throw remote_close();

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

  typedef boost::tuple<std::string, std::string, std::string> request_line;
  enum { REQUEST_METHOD, REQUEST_URI, REQUEST_HTTP_VERSION };

  template<class Source>
  void get_until(char end, Source &in, std::string &ret) {
    int t;
    while( (t = io::get(in)) != end) {
      if(t == Source::traits_type::eof())
        throw remote_close();
      ret += t;
    }
  }

  template<class Source>
  request_line get_request_line(Source &in) {
    request_line ret;
    get_until(' ', in, ret.get<REQUEST_METHOD>());
    get_until(' ', in, ret.get<REQUEST_URI>());
    get_until('\r', in, ret.get<REQUEST_HTTP_VERSION>());
    if(!expect(in, '\n'))
      throw bad_format();
    return ret;
  }

  void assure_relative_uri(std::string &uri) {
    typedef boost::iterator_range<std::string::iterator> spart;
    spart scheme = algo::find_first(uri, "://");
    if (scheme.empty())
      return;
    spart rest(scheme.end(), uri.end());
    spart abs = algo::find_first(rest, "/");
    uri.assign(abs.begin(), uri.end());
  }
}

response http_connection::handle_request(hosts_cont_t const &hosts) {
  try {
    std::string method, uri, version;
    boost::tie(method, uri, version) = get_request_line(conn);

    std::cout << method << " " << uri << " " << version << "\n"; // DEBUG

    if(version == "HTTP/1.0") {
      flags.set(HTTP_1_0_COMPAT);
      open_ = false;
    }
    else if(version != "HTTP/1.1")
      return 505; // HTTP Version not Supported

    header_fields fields = read_headers();

    hosts_cont_t::const_iterator host = find_host(fields, hosts);
    if (host == hosts.end())
      return 404;
    context &global = host->get().get_context();

    keywords kw;

    det::any_path path_id;
    det::responder_base *responder;
    context *local;
    assure_relative_uri(uri);
    std::cout << "?-uri " << uri << '\n';//DEBUG
    global.find_responder(uri, path_id, responder, local, kw);

    if (!responder)
      return 404;

    if(!flags.test(HTTP_1_0_COMPAT)) {
      header_fields::iterator connect_header = fields.find("connection");
      if(connect_header != fields.end() && connect_header->second == "close")
        open_ = false;
    }

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
    else if(method == "POST") {
      det::poster_base *poster = responder->x_poster();
      if (!poster || !responder->x_exists(path_id, kw))
        return 404;

      int ret = handle_entity(kw, fields);
      if(ret != 0)
        return ret;

      return poster->x_post(path_id, kw);
    }
    else if(method == "PUT") {
      det::putter_base *putter = responder->x_putter();
      if (!putter)
        return 404;

      int ret = handle_entity(kw, fields);
      if(ret != 0)
        return ret;

      return putter->x_put(path_id, kw);
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
      data += "\r\nEntity-Data not included!\r\n";
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

http_connection::header_fields http_connection::read_headers() {
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
      std::cout << ret.first->first << ": "
                << ret.first->second << "\n";
    else
      std::cout << "field not added!\n";

    if(expect(conn, '\r') && expect(conn, '\n'))
      break;
  }
  return fields;
}

namespace {
  enum {
    CSV_HEADERS=4
  };
  char const * const csv_header[CSV_HEADERS] = {
    "accept", "accept-charset", "accept-encoding",
    "accept-language"   // TODO ...
  };
  char const * const * csv_header_end = csv_header + CSV_HEADERS;
}

// reads a header field from `in' and adds it to `fields'
// see RFC 2616 chapter 4.2
// Warning: Field names are converted to all lower-case!
template<class Source>
std::pair<http_connection::header_fields::iterator, bool> 
http_connection::get_header_field(Source &in, header_fields &fields) {
  std::string name;
  int t = 0;
  for(;;) {
    t = io::get(in);
    if(t == '\n' || t == '\r')
      throw bad_format();
    else if(t == Source::traits_type::eof())
      throw remote_close();
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
      throw remote_close();
    else
      value += t;
  }
  std::pair<http_connection::header_fields::iterator, bool> ret =
    fields.insert(std::make_pair(name, value));
  if(!ret.second &&
     std::find(csv_header, csv_header_end, name) != csv_header_end)
  {
    fields[name] += std::string(", ") + value;
    std::cout << "field " << name << " value: " << fields[name] << "\n"; //DEBUG
  }
  return ret;
}

hosts_cont_t::const_iterator 
http_connection::find_host(
    header_fields const &fields, hosts_cont_t const &hosts)
{
  header_fields::const_iterator host_header = fields.find("host");
  if(host_header == fields.end())
    throw bad_format();

  std::string::const_iterator begin = host_header->second.begin();
  std::string::const_iterator end = host_header->second.end();
  std::string::const_iterator delim = std::find(begin, end, ':');

  std::string the_host(begin, delim);

  hosts_cont_t::const_iterator it = hosts.find(the_host);
  while(it == hosts.end() &&
        !the_host.empty())
  {
    std::string::const_iterator begin = the_host.begin();
    std::string::const_iterator end = the_host.end();
    std::string::const_iterator delim = std::find(begin, end, '.');

    if (delim == end)
      the_host.clear();
    else
      the_host.assign(++delim, end);

    it = hosts.find(the_host);
  }
  return it;
}

namespace {
  class pop_filt_stream : public std::istream {
  public:
    typedef io::filtering_streambuf<io::input> buf_t;

    explicit pop_filt_stream(buf_t *buf = new buf_t) {
      rdbuf(buf);
    }

    ~pop_filt_stream() {
      if (buf) {
        std::cout << "deletering" << std::endl;
        ignore(std::numeric_limits<int>::max());
        std::cout << "pop conn" << std::endl;
        buf->pop();
        std::cout << "~buf" << std::endl;
        delete buf;
      }
    }

    buf_t &filt() { return *buf; }

    buf_t *reset() {
      buf_t *ptr = buf;
      buf = 0;
      return ptr;
    }

  private:
    buf_t *buf;
  };
}

int http_connection::handle_entity(keywords &kw, header_fields &fields) {
  std::cout << "handling entity" << std::endl;

  header_fields::iterator transfer_encoding =
    fields.find("transfer-encoding");
  bool has_transfer_encoding = transfer_encoding != fields.end();

  std::cout << "te.. " << has_transfer_encoding << std::endl;

  pop_filt_stream fin;

  std::cout << "got past 'tor" << std::endl;

  if(has_transfer_encoding) {
    std::cout << "TE: " << transfer_encoding->second << std::endl; // DEBUG
    if(algo::iequals(transfer_encoding->second, "chunked"))
      fin.filt().push(utils::chunked_filter());
    else
      return 501;
  }

  std::cout << "check ce" << std::endl;

  header_fields::iterator content_encoding = fields.find("content-encoding");
  if(content_encoding != fields.end()) {
    std::cout << "  .. " << content_encoding->second << std::endl;
    if(algo::iequals(content_encoding->second, "gzip"))
      fin.filt().push(io::gzip_decompressor());
    else if(algo::iequals(content_encoding->second, "bzip2"))
      fin.filt().push(io::bzip2_decompressor());
  }

  std::cout << "check the cle" << std::endl;
 
  header_fields::iterator content_length = fields.find("content-length");
  if(content_length == fields.end() && !has_transfer_encoding)
    return 411; // Content-length required
  else {
    std::cout << "feat the cle" << std::endl;
    std::size_t const length =
      boost::lexical_cast<std::size_t>(content_length->second);
    std::cout << "length: " << length << std::endl;
    fin.filt().push(utils::length_filter(length)); // Problem: BLOCKT!!
    std::cout << "yep." << std::endl;
  }

  std::cout << "FEAT ALL FILT" << std::endl;

  fin.filt().push(boost::ref(conn), 0, 0);

  std::cout << "this the connref place" << std::endl;

  if(!flags.test(HTTP_1_0_COMPAT)) {
    header_fields::iterator expect = fields.find("expect");
    if (expect != fields.end()) {
      if (!algo::iequals(expect->second, "100-continue"))
        return 417;
      send(100, false);
    }
  }

  std::cout << "set entity" << std::endl;

  kw.set_entity(new pop_filt_stream(fin.reset()));

  return 0;
}

void http_connection::send(response const &r, bool entity) {
  //TODO implement partial-GET, entity data from streams

  io::filtering_ostream out(boost::ref(conn), 0, 0);

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
  if (!r.get_data().empty() && entity) {
    io::filtering_ostream out2;
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

#if 0
//---------------
// TESTS

#include <testsoon.hpp>

#if 0
TEST_GROUP(http) {

}
#endif

TEST_GROUP(aux) {
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
    header += "\r\n";
    std::stringstream x(header);

    typedef http_connection::header_fields header_fields;
    header_fields fields;
    std::pair<header_fields::iterator, bool> field = 
      http_connection::get_header_field(x, fields);

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

  XTEST((values, (char)('a')('1')('F'))) {
    std::stringstream s;
    s << value;
    int x;
    s >> std::hex >> x;
    boost::tuple<bool, int> t = utils::hex2int(value);
    Check(t.get<0>());
    Equals(t.get<1>(), x);
  }
}
#endif


