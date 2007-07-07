// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include "rest-config.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

#include <boost/lexical_cast.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include <boost/ref.hpp>

#include <cstring>
#include <bitset>
#include <map>
#include <limits>
#include <fstream>

#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#ifdef APPLE
#include "epoll.h"
#else
#include <sys/epoll.h>
#endif

#include <iostream>//DEBUG

#ifndef NDEBUG //whatever
#define NO_FORK_LOOP
#endif

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

class server::socket_param::impl {
public:
  impl(short port, socket_type_t type, std::string const &bind)
    : service(boost::lexical_cast<std::string>(port)), socket_type(type),
      bind(bind), fd(-1)
  { }
  impl(std::string const &service, socket_type_t type, std::string const &bind)
    : service(service), socket_type(type), bind(bind), fd(-1)
  { }

  std::string service;
  socket_type_t socket_type;
  std::string bind;
  hosts_cont_t hosts;
  
  int fd;
};

server::socket_param::socket_param(short port, socket_type_t type,
                                   std::string const &bind)
  : p(new impl(port, type, bind))
{ }

server::socket_param::socket_param(std::string const &service,
                                   socket_type_t type,
                                   std::string const &bind)
  : p(new impl(service, type, bind))
{ }

server::socket_param::~socket_param() { }

int server::socket_param::fd() const {
  return p->fd;
}
void server::socket_param::fd(int f) {
  p->fd = f;
} 

std::string const &server::socket_param::service() const {
  return p->service;
}

std::string const &server::socket_param::bind() const {
  return p->bind;
}

server::socket_param::socket_type_t
server::socket_param::socket_type() const {
  return p->socket_type;
}

void server::socket_param::add_host(host const &h) {
  if (!p->hosts.insert(boost::ref(h)).second)
    throw std::logic_error("cannot serve two hosts with same name");
}

host const *server::socket_param::get_host(std::string const &name) const {
  std::string::const_iterator begin = name.begin();
  std::string::const_iterator end = name.end();
  std::string::const_iterator delim = std::find(begin, end, ':');

  std::string the_host(begin, delim);

  hosts_cont_t::const_iterator it = p->hosts.find(the_host);
  while(it == p->hosts.end() &&
        !the_host.empty())
  {
    std::string::const_iterator begin = the_host.begin();
    std::string::const_iterator end = the_host.end();
    std::string::const_iterator delim = std::find(begin, end, '.');

    if (delim == end)
      the_host.clear();
    else
      the_host.assign(++delim, end);

    it = p->hosts.find(the_host);
  }
  if(it == p->hosts.end())
    return 0x0;
  return it->get_pointer();
}

class server::impl {
public:
  std::vector<socket_param> socket_params;
  std::set<int> close_on_fork;

  static int const DEFAULT_LISTENQ;
  int listenq;

  utils::property_tree const &config;

  void do_close_on_fork() {
    for(std::set<int>::const_iterator i = close_on_fork.begin();
        i != close_on_fork.end();
        ++i)
      ::close(*i);
  }

  impl(utils::property_tree const &config)
    : listenq(utils::get(config, DEFAULT_LISTENQ, "connections", "listenq")),
      config(config)
  {
    read_connections();
  }

  void read_connections() {
    typedef utils::property_tree::children_iterator children_iterator;
    children_iterator end = config.children_end();
    children_iterator i = utils::get(config, end, "connections");
    if(i == config.children_end()) {
      REST_LOG(utils::INFO, "no connections specified in config-file");
    }
    else
      for(children_iterator j = (*i)->children_begin();
          j != (*i)->children_end();
          ++j)
        {
          std::string service = utils::get(**j, std::string(), "port");
          if(service.empty()) {
            service = utils::get(**j, std::string(), "service");
            if(service.empty())
              throw std::runtime_error("no port/service specified!");
          }
          algo::trim(service);
          std::string type_ = utils::get(**j, std::string("ipv4"), "type");
          socket_param::socket_type_t type;
          if(algo::istarts_with(type_, "ipv4") ||
             algo::istarts_with(type_, "ip4"))
            type = socket_param::ip4;
          else if(algo::istarts_with(type_, "ipv6") ||
                  algo::istarts_with(type_, "ip6"))
            type = socket_param::ip6;
          else
            throw std::runtime_error("unkown socket type specified");
          
          std::string bind = utils::get(**j, std::string(), "bind");
          algo::trim(bind);

          std::cout << "SOCKET " << service << ' ' << type << ' ' <<
            bind << '\n';
          
          socket_params.push_back(socket_param(service, type, bind));
        }
  }

  static void sigchld_handler(int) {
    while (::waitpid(-1, 0, WNOHANG) > 0)
      ;
  }

  static bool restart;
  static void restart_handler(int) {
    restart = true;
  }

  int initialize_sockets();

  void incoming(socket_param const &sock);
};

bool server::impl::restart = false;
int const server::impl::DEFAULT_LISTENQ = 5;

server::sockets_iterator server::add_socket(socket_param const &s) {
  p->socket_params.push_back(s);
  return --p->socket_params.end();
}

server::sockets_iterator server::sockets_begin() {
  return p->socket_params.begin();
}
server::sockets_iterator server::sockets_end() {
  return p->socket_params.end();
}
server::sockets_const_iterator server::sockets_begin() const {
  return p->socket_params.begin();
}
server::sockets_const_iterator server::sockets_end() const {
  return p->socket_params.end();
}

void server::sockets_erase(sockets_iterator i) {
  p->socket_params.erase(i);
}
void server::sockets_erase(sockets_iterator begin, sockets_iterator end) {
  p->socket_params.erase(begin, end);
}

void server::set_listen_q(int no) {
  p->listenq = no;
}

server::server(utils::property_tree const &conf) : p(new impl(conf)) { }
server::~server() { }

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
    std::vector<response::content_encoding_t> encodings;

  public:
    http_connection(connection_streambuf &conn)
      : conn(conn), open_(true) {}

    bool open() const { return open_; }

    typedef utils::http::header_fields header_fields;

    int set_header_options(header_fields &fields);

    void reset() {
      flags.reset();
      encodings.clear();
    }

    void handle_request(server::socket_param const &hosts, response &o);
    int handle_entity(keywords &kw, header_fields &fields);

    void send(response const &r, bool entity);
    void send(response const &r) {
      send(r, !flags.test(NO_ENTITY));
    }

    static hosts_cont_t::const_iterator find_host(
      header_fields const &fields, hosts_cont_t const &hosts);

    void serve(server::socket_param const &sock);
  };
}

namespace {
  void close_on_exec(int fd) {
    if(::fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
      throw utils::errno_error("fcntl");
  }

  namespace epoll {
    int create(int size) {
      int epollfd = ::epoll_create(size);
      if(epollfd == -1)
        throw utils::errno_error("could not start server (epoll_create)");
      close_on_exec(epollfd);
      return epollfd;
    }

    int wait(int epollfd, epoll_event *events, int maxevents) {
      int nfds = ::epoll_wait(epollfd, events, maxevents, -1);
      if(nfds == -1) {
        if(errno == EINTR)
          return 0;
        throw utils::errno_error("could not run server (epoll_wait)");
      }
      return nfds;
    }
  }

  int socket(int type) {
    int sock = ::socket(type, SOCK_STREAM, 0);
    if(sock == -1)
      throw utils::errno_error("could not start server (socket)");
    close_on_exec(sock);
    return sock;
  }

  void getaddrinfo(server::sockets_iterator i, addrinfo **res) {
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = i->socket_type();
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    char const *hostname = i->bind().empty() ? 0x0 : i->bind().c_str();
    int n = ::getaddrinfo(hostname, i->service().c_str(), &hints, res);
    if(n != 0)
      throw std::runtime_error(std::string("getaddrinfo failed: ") +
                               gai_strerror(n));
  }

  int create_listenfd(server::sockets_iterator i, int backlog) {
    addrinfo *res;
    getaddrinfo(i, &res);
    addrinfo *const ressave = res;

    int listenfd;
    do {
      listenfd = socket(i->socket_type());

      int const one = 1;
      setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

      if(::bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
        break;

      ::close(listenfd);
    } while( (res = res->ai_next) != 0x0 );
    ::freeaddrinfo(ressave);

    if(res == 0x0)
      throw utils::errno_error("could not start server (listen)");

    if(::listen(listenfd, backlog) == -1)
      throw utils::errno_error("could not start server (listen)");

    i->fd(listenfd);

    return listenfd;
  }
}

int server::impl::initialize_sockets() {
  int epollfd = epoll::create(socket_params.size() + 1);
  close_on_fork.insert(epollfd);

  epoll_event epolle;
  epolle.events = EPOLLIN|EPOLLERR;

  for(sockets_iterator i = socket_params.begin();
      i != socket_params.end();
      ++i)
  {
    int listenfd = create_listenfd(i, listenq);

    close_on_fork.insert(listenfd);

    epolle.data.ptr = &*i;
    if(::epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &epolle) == -1)
      throw utils::errno_error("could not start server (epoll_ctl)");
  }

  return epollfd;
}

namespace {
  std::vector<char*> getargs(std::string const &path, std::string &data) {
    std::ifstream in(path.c_str());
    data.assign(std::istreambuf_iterator<char>(in.rdbuf()),
                std::istreambuf_iterator<char>());

    std::vector<char*> ret;
    ret.push_back(&data[0]);
    std::size_t const size = data.size();

    for(std::size_t i = 0; i < size; ++i)
      if(data[i] == '\0' && i+1 < size)
        ret.push_back(&data[i+1]);
    ret.push_back(0);

    return ret;
  }

  void do_restart() {
    REST_LOG(utils::IMPORTANT, "server restart");

    std::string cmdbuffer;
    std::string envbuffer;

    char resolved_cmd[8192];
    int n = readlink("/proc/self/exe", resolved_cmd, sizeof(resolved_cmd) - 1);
    if (n < 0) {
      REST_LOG_ERRNO(utils::CRITICAL, "restart failed (readlink)");
      return;
    }
    resolved_cmd[n] = '\0';

    if(::execve(resolved_cmd, &getargs("/proc/self/cmdline", cmdbuffer)[0],
                &getargs("/proc/self/environ", envbuffer)[0]) == -1)
      REST_LOG_ERRNO(utils::CRITICAL, "restart failed (execve)");
  }
}

void server::serve() {
  typedef void(*sighnd_t)(int);

  sighnd_t oldchld = ::signal(SIGCHLD, &impl::sigchld_handler);
  ::siginterrupt(SIGCHLD, 0);

  sighnd_t oldhup = ::signal(SIGUSR1, &impl::restart_handler);
  ::siginterrupt(SIGUSR1, 0);

  int epollfd = p->initialize_sockets();

  int const EVENTS_N = 100;

  for(;;) {
    epoll_event events[EVENTS_N];
    int nfds = epoll::wait(epollfd, events, EVENTS_N);
    if(impl::restart)
      do_restart();
    for(int i = 0; i < nfds; ++i) {
      socket_param *ptr = static_cast<socket_param*>(events[i].data.ptr);
      assert(ptr);

      p->incoming(*ptr);
    }
  }

  ::signal(SIGHUP, oldhup);
  ::signal(SIGCHLD, oldchld);
}

void server::impl::incoming(server::socket_param const &sock)  {
  sockaddr_in cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  int connfd = ::accept(sock.fd(), (sockaddr *) &cliaddr, &clilen);
  if(connfd == -1) {
    REST_LOG_ERRNO(utils::CRITICAL, "accept failed");
    return;
  }

  std::cout << "%% ACCEPTED" << std::endl; // DEBUG
#ifndef NO_FORK_LOOP
  sigset_t mask, oldmask;
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);

  pid_t pid = ::fork();
  if (pid == 0) {
    do_close_on_fork();
    int status = 0;
#endif
    try {
      connection_streambuf buf(connfd, 10);
      http_connection conn(buf);
      conn.serve(sock);
      std::cout << "%% CLOSING" << std::endl; // DEBUG
    }
#if 0
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
#else
    catch (int) { throw; }
#endif

#ifndef NO_FORK_LOOP
    exit(status);
  }
  else {
    if (pid == -1)
      REST_LOG_ERRNO(utils::CRITICAL, "fork failed");
    close(connfd);
    sigprocmask(SIG_SETMASK, &oldmask, 0);
  }
#endif
}

void http_connection::serve(server::socket_param const &sock) {
  try {
    while (open()) {
      reset();
      response r(response::empty_tag());
      handle_request(sock, r);
      send(r);
    }
  }
  catch (utils::http::remote_close&) {
    std::cout << "%% remote or timeout" << std::endl; // DEBUG
  }
}

namespace {
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

void http_connection::handle_request(
    server::socket_param const &sock, response &out)
{
  try {
    std::string method, uri, version;
    boost::tie(method, uri, version) = utils::http::get_request_line(conn);

    std::cout << method << " " << uri << " " << version << "\n"; // DEBUG

    if(version == "HTTP/1.0") {
      flags.set(HTTP_1_0_COMPAT);
      open_ = false;
    }
    else if(version != "HTTP/1.1")
      throw 505; // HTTP Version not Supported

    header_fields fields = utils::http::read_headers(conn);

    int ret = set_header_options(fields);
    if(ret != 200)
      throw ret;

    header_fields::const_iterator host_header = fields.find("host");
    if(host_header == fields.end())
      throw utils::http::bad_format();

    host const *h = sock.get_host(host_header->second);
    if(!h)
      throw 404;
    context &global = h->get_context();

    keywords kw;

    det::any_path path_id;
    det::responder_base *responder;
    context *local;
    assure_relative_uri(uri);
    std::cout << "?-uri " << uri << '\n';//DEBUG
    global.find_responder(uri, path_id, responder, local, kw);

    if (!responder)
      throw 404;

    if(!flags.test(HTTP_1_0_COMPAT)) {
      header_fields::iterator connect_header = fields.find("connection");
      if(connect_header != fields.end() && connect_header->second == "close")
        open_ = false;
    }

    if (method == "GET") {
      det::getter_base *getter = responder->x_getter();
      if (!getter || !responder->x_exists(path_id, kw))
        throw 404;
      getter->x_get(path_id, kw).move(out);
      return;
    }
    else if(method == "HEAD") {
      flags.set(NO_ENTITY);
      det::getter_base *getter = responder->x_getter();
      if (!getter || !responder->x_exists(path_id, kw))
        throw 404;

      getter->x_get(path_id, kw).move(out);
      return;
    }
    else if(method == "POST") {
      det::poster_base *poster = responder->x_poster();
      if (!poster || !responder->x_exists(path_id, kw))
        throw 404;

      int ret = handle_entity(kw, fields);
      if(ret != 0)
        throw ret;

      poster->x_post(path_id, kw).move(out);
      return;
    }
    else if(method == "PUT") {
      det::putter_base *putter = responder->x_putter();
      if (!putter)
        throw 404;

      int ret = handle_entity(kw, fields);
      if(ret != 0)
        throw ret;

      putter->x_put(path_id, kw).move(out);
      return;
    }
    else if (method == "DELETE") {
      det::deleter_base *deleter = responder->x_deleter();
      if (!deleter || !responder->x_exists(path_id, kw))
        throw 404;
      deleter->x_delete(path_id, kw).move(out);
      return;
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
      ret.move(out);
#else
      throw 501;
#endif
    }
    else if (method == "CONNECT" || method == "OPTIONS")
      throw 501; // Not Supported
    else
      throw 400; // Bad Request
  } catch (utils::http::bad_format &) {
    response(400).move(out); // Bad Request
  } catch (int i) {
    response(i).move(out);
  }
}

int http_connection::set_header_options(header_fields &fields) {
  typedef std::multimap<int, std::string> qlist_t;

  qlist_t qlist;
  utils::http::parse_qlist(fields["accept-encoding"], qlist);

  qlist_t::const_reverse_iterator const rend = qlist.rend();
  bool found = false;
  for(qlist_t::const_reverse_iterator i = qlist.rbegin(); i != rend; ++i) {
    if(i->first == 0) {
      if(!found && (i->second == "identity" || i->second == "*"))
        return 406;
    }
    else {
      if(i->second == "gzip" || i->second == "x-gzip") {
        encodings.push_back(response::gzip);
        found = true;
      }
      else if(i->second == "bzip2" || i->second == "x-bzip2") {
        encodings.push_back(response::bzip2);
        found = true;
      }
      else if(i->second == "identity")
        found = true;
    }
  }

  return 200;
}

namespace {
  class pop_filt_stream : public std::istream {
  public:
    typedef io::filtering_streambuf<io::input> buf_t;

    explicit pop_filt_stream(buf_t *buf = new buf_t) : buf(buf) {
      rdbuf(buf);
    }

    ~pop_filt_stream() {
      if (buf) {
        if (buf->is_complete()) {
          ignore(std::numeric_limits<int>::max());
          buf->pop();
        }
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
  header_fields::iterator transfer_encoding =
    fields.find("transfer-encoding");
  bool has_transfer_encoding = transfer_encoding != fields.end();

  pop_filt_stream fin;

  if(has_transfer_encoding) {
    if(algo::iequals(transfer_encoding->second, "chunked"))
      fin.filt().push(utils::chunked_filter());
    else
      return 501;
  }

  header_fields::iterator content_encoding = fields.find("content-encoding");
  if(content_encoding != fields.end()) {
    if(algo::iequals(content_encoding->second, "gzip") ||
       (flags.test(HTTP_1_0_COMPAT) &&
        algo::iequals(content_encoding->second, "x-gzip")))
      fin.filt().push(io::gzip_decompressor());
    else if(algo::iequals(content_encoding->second, "bzip2"))
      fin.filt().push(io::bzip2_decompressor());
  }

  header_fields::iterator content_length = fields.find("content-length");
  if(content_length == fields.end() && !has_transfer_encoding)
    return 411; // Content-length required
  else {
    std::size_t const length =
      boost::lexical_cast<std::size_t>(content_length->second);
    fin.filt().push(utils::length_filter(length));
  }

  if(!flags.test(HTTP_1_0_COMPAT)) {
    header_fields::iterator expect = fields.find("expect");
    if (expect != fields.end()) {
      if (!algo::iequals(expect->second, "100-continue"))
        return 417;
      send(100, false);
    }
  }

  fin.filt().push(boost::ref(conn), 0, 0);

  header_fields::iterator content_type = fields.find("content-type");

  std::auto_ptr<std::istream> pstream(new pop_filt_stream(fin.reset()));
  kw.set_entity(
      pstream,
      content_type == fields.end() ?
        "application/octet-stream" :
        content_type->second
  );

  return 0;
}

class noflush_writer {
public:
  typedef char char_type;

  struct category
    :
      io::sink_tag
  {};

  noflush_writer(std::streambuf *buf) : buf(buf) {}

  std::streamsize write(char const *data, std::streamsize length) {
    return io::write(*buf, data, length);
  }

  void real_flush() {
    io::flush(*buf);
  }

private:
  std::streambuf *buf;
};

void http_connection::send(response const &r, bool entity) {
  std::cout << "enc ";
  for (std::vector<response::content_encoding_t>::iterator it = encodings.begin(); it != encodings.end(); ++it)
    std::cout << *it << (it+1 == encodings.end() ? '\n' : ',');

  //TODO implement partial-GET, entity data from streams
  //TODO BUG: gzip does not work with w3m!

  conn->push_cork();

  io::stream<noflush_writer> out(&conn);

  if (flags.test(HTTP_1_0_COMPAT))
    out << "HTTP/1.0 ";
  else
    out << "HTTP/1.1 ";

  int code = r.get_code();
  if (code == -1)
    code = 200;

  std::cout << "Send: " << code << " CE:" << (flags.test(ACCEPT_GZIP) ? "gzip" : (flags.test(ACCEPT_BZIP2) ? "bzip2" : "none")) << "\n"; //DEBUG

  out << code << " " << response::reason(code) << "\r\n";

  out << "Date: " << utils::http::current_date_time()  << "\r\n";
  out << "Server: " << REST_SERVER_ID << "\r\n";

  if (!r.get_type().empty()) 
    out << "Content-Type: " << r.get_type() << "\r\n";
  if (!entity)
    out << "\r\n";

  else {
    bool may_chunk = !flags.test(HTTP_1_0_COMPAT);

    response::content_encoding_t enc = 
        r.choose_content_encoding(encodings, may_chunk);

    switch (enc) {
    case response::gzip:
      out << "Content-Encoding: gzip\r\n";
      break;
    case response::bzip2:
      out << "Content-Encoding: bzip2\r\n";
      break;
    default: break;
    }

    if (!r.chunked(enc))
      out << "Content-Length: " << r.length(enc) << "\r\n";
    else if (may_chunk)
      out << "Transfer-Encoding: chunked\r\n";
    out << "\r\n";

    r.print(out, enc, may_chunk);
  }

  io::flush(out);
  out->real_flush();

  conn->pull_cork();
}

#if 0
//---------------
// TESTS

#include <testsoon.hpp>

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


