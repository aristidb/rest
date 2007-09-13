// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include "rest-config.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/operations.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include <boost/ref.hpp>

#include <ctime>
#include <cstdlib>
#include <cstring>
#include <bitset>
#include <map>
#include <limits>
#include <fstream>
#include <sstream>

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
#include <grp.h>


#ifdef APPLE
#include "epoll.h"
#else
#include <sys/epoll.h>
#endif

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

typedef
  boost::multi_index_container<
    boost::reference_wrapper<host const>,
    indexed_by<
      hashed_unique<const_mem_fun<host, std::string, &host::get_host> > > >
  hosts_cont_t;

class server::socket_param::impl {
public:
  impl(
      std::string const &service,
      socket_type_t type,
      std::string const &bind,
      long timeout_read,
      long timeout_write)
  : service(service),
    socket_type(type),
    bind(bind),
    timeout_read(timeout_read),
    timeout_write(timeout_write),
    fd(-1)
  { }

  std::string service;
  socket_type_t socket_type;
  std::string bind;
  long timeout_read;
  long timeout_write;

  hosts_cont_t hosts;
  
  int fd;
};

server::socket_param::socket_param(
    std::string const &service,
    socket_type_t type,
    std::string const &bind,
    long timeout_read,
    long timeout_write
  )
: p(new impl(service, type, bind, timeout_read, timeout_write))
{ }

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

long server::socket_param::timeout_read() const {
  return p->timeout_read;
}

long server::socket_param::timeout_write() const {
  return p->timeout_write;
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
  static long const DEFAULT_TIMEOUT;
  int listenq;
  long timeout_read;
  long timeout_write;

  utils::property_tree const &config;

  void do_close_on_fork() {
    std::for_each(close_on_fork.begin(), close_on_fork.end(), &::close);
  }

  impl(utils::property_tree const &config)
    : listenq(utils::get(config, DEFAULT_LISTENQ, "connections", "listenq")),
      timeout_read(utils::get(config, DEFAULT_TIMEOUT,
          "connections", "timeout", "read")),
      timeout_write(utils::get(config, DEFAULT_TIMEOUT,
          "connections", "timeout", "write")),
      config(config)
  {
    read_connections();
  }

  static void sigchld_handler(int) {
    while (::waitpid(-1, 0, WNOHANG) > 0)
      ;
  }

  static bool restart;
  static void restart_handler(int) {
    restart = true;
  }

  void read_connections();
  int initialize_sockets();
  void incoming(socket_param const &sock, std::string const &severname);
  int connection(socket_param const &sock, int connfd, std::string const &name);
};

bool server::impl::restart = false;
int const server::impl::DEFAULT_LISTENQ = 5;
long const server::impl::DEFAULT_TIMEOUT = 10;

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
    server::socket_param const &sock;
    int connfd;
    connection_streambuf conn;

    std::string const &servername;

    bool open_;
    enum {
      NO_ENTITY,
      HTTP_1_0_COMPAT,
      X_NO_FLAG
    };
    typedef std::bitset<X_NO_FLAG> state_flags;
    state_flags flags;
    std::vector<response::content_encoding_t> encodings;

    request request_;

  public:
    http_connection(server::socket_param const &sock, int connfd,
                    std::string const &servername)
    : sock(sock),
      connfd(connfd),
      conn(connfd, sock.timeout_read(), sock.timeout_write()),
      servername(servername),
      open_(true)
    { }

    bool open() const { return open_; }

    int set_header_options();

    void reset() {
      flags.reset();
      encodings.clear();
    }

    response handle_request();
    int handle_entity(keywords &kw);

    void send(response r, bool entity);
    void send(response r) {
      send(r, !flags.test(NO_ENTITY));
    }

    static hosts_cont_t::const_iterator find_host(
      utils::http::header_fields const &fields, hosts_cont_t const &hosts);

    void serve();

    int handle_modification_tags(
      time_t, std::string const &, std::string const &);

    response handle_get(
      det::responder_base*, det::any_path const&, keywords&, request const&);
    response handle_head(
      det::responder_base*, det::any_path const&, keywords&, request const&);
    response handle_post(
      det::responder_base*, det::any_path const&, keywords&, request const&);
    response handle_put(
      det::responder_base*, det::any_path const&, keywords&, request const&);
    response handle_delete(
      det::responder_base*, det::any_path const&, keywords&, request const&);
  };

  typedef boost::function<
    response (
      http_connection *,
      det::responder_base *,
      det::any_path const &,
      keywords &,
      request const &
    )
  > method_handler;

  method_handler H(
      response (http_connection::*fun)(
        det::responder_base*, det::any_path const&, keywords&, request const&
      )
    )
  {
    return boost::bind(fun, _1, _2, _3, _4, _5);
  }
  typedef std::map<std::string, method_handler> method_handler_map;

  static std::pair<std::string, method_handler> method_handlers_raw[] = {
    std::make_pair("GET", H(&http_connection::handle_get)),
    std::make_pair("HEAD", H(&http_connection::handle_head)),
    std::make_pair("POST", H(&http_connection::handle_post)),
    std::make_pair("PUT", H(&http_connection::handle_put)),
    std::make_pair("DELETE", H(&http_connection::handle_delete)),
  };

  static method_handler_map const method_handlers(
    method_handlers_raw,
    method_handlers_raw + 
      sizeof(method_handlers_raw) / sizeof(*method_handlers_raw)
  );
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
      ::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

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

    utils::log(LOG_NOTICE,
               "created %s socket on %s:%s (timeouts r: %ld w: %ld)",
               i->socket_type() == server::socket_param::ip4 ? "IPv4" : "IPv6",
               i->bind().c_str(), i->service().c_str(), i->timeout_read(),
               i->timeout_write());

    return listenfd;
  }
}

void server::impl::read_connections() {
  typedef utils::property_tree::children_iterator children_iterator;
  children_iterator end = config.children_end();
  children_iterator i = utils::get(config, end, "connections");
  if(i == config.children_end()) {
    utils::log(LOG_WARNING, "no connections container");
    return;
  }

  for(children_iterator j = (*i)->children_begin();
      j != (*i)->children_end();
      ++j)
  {
    if ((*j)->name() == "timeout")
      continue;

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

    long timeout_read =
      utils::get(**j, this->timeout_read, "timeout", "read");
    long timeout_write =
      utils::get(**j, this->timeout_write, "timeout", "write");
          
    socket_params.push_back(socket_param(
      service, type, bind, timeout_read, timeout_write));
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
    utils::log(LOG_NOTICE, "server restart");

    std::string cmdbuffer;
    std::string envbuffer;

    char resolved_cmd[8192];
    int n = readlink("/proc/self/exe", resolved_cmd, sizeof(resolved_cmd) - 1);
    if (n < 0) {
      utils::log(LOG_ERR, "restart failed: readlink: %m");
      return;
    }
    resolved_cmd[n] = '\0';

    if(::execve(resolved_cmd, &getargs("/proc/self/cmdline", cmdbuffer)[0],
                &getargs("/proc/self/environ", envbuffer)[0]) == -1)
    {
      utils::log(LOG_ERR, "restart failed: execve: %m");
    }
  }

  bool set_gid(gid_t gid) {
    if(::setgroups(1, &gid) == -1) {
      if(errno != EPERM)
        throw utils::errno_error("setgroups failed");
      return false;
    }
    return true;
  }

  bool set_uid(uid_t uid) {
    if(::setuid(uid) == -1) {
      if(errno != EPERM)
        throw utils::errno_error("setuid failed");
      return false;
    }
    return true;
  }

  void drop_privileges(utils::property_tree const &tree) {
    long gid = utils::get(tree, -1, "general", "gid");
    if(gid != -1)
      set_gid(gid);
    else
      utils::log(LOG_WARNING, "no gid set: group privileges not droped");

    long uid = utils::get(tree, -1, "general", "uid");
    if(uid != -1)
      set_uid(uid);
    else
      utils::log(LOG_WARNING, "no uid set: user privilieges not droped");
  }

  void term_handler(int sig) {
    utils::log(LOG_NOTICE, "server is going down (SIG %d)", sig);
    exit(4);
  }
}

void server::serve() {
  utils::log(LOG_NOTICE, "server started");

  utils::property_tree &tree = config::get().tree();

#ifndef DEBUG
  if(::chroot(".") == -1) {
    if(errno != EPERM)
      throw utils::errno_error("chroot failed");
    else
      utils::log(LOG_WARNING, "could not chroot: insufficient permissions");
  }
  drop_privileges(tree);

  if(::daemon(1, 1) == -1)
    throw utils::errno_error("daemonizing the server failed (daemon)");
#endif

  typedef void(*sighnd_t)(int);

  ::signal(SIGTERM, &term_handler);
  ::siginterrupt(SIGTERM, 0);

  ::signal(SIGCHLD, &impl::sigchld_handler);
  ::siginterrupt(SIGCHLD, 0);

  ::signal(SIGUSR1, &impl::restart_handler);
  ::siginterrupt(SIGUSR1, 0);

  // TODO SIGPIPE auf ignore setzen?

  std::string const &servername =
    utils::get(tree, std::string(), "general", "name");
  // shouldn't require a default value (see config::config())
  assert(!servername.empty());

  int epollfd = p->initialize_sockets();

  int const EVENTS_N = 8;

  for(;;) {
    epoll_event events[EVENTS_N];
    int nfds = epoll::wait(epollfd, events, EVENTS_N);
    if(impl::restart)
      do_restart();
    for(int i = 0; i < nfds; ++i) {
      socket_param *ptr = static_cast<socket_param*>(events[i].data.ptr);
      assert(ptr);
      p->incoming(*ptr, servername);
    }
  }
}

void server::impl::incoming(server::socket_param const &sock,
                            std::string const &servername)
{
  sockaddr_in cliaddr;
  socklen_t clilen = sizeof(cliaddr);
  int connfd = ::accept(sock.fd(), (sockaddr *) &cliaddr, &clilen);
  if(connfd == -1) {
    utils::log(LOG_ERR, "accept failed: %m");
    return;
  }

#ifndef NO_FORK_LOOP
  sigset_t mask, oldmask;
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);

  pid_t pid = ::fork();
  if (pid == 0) {
    do_close_on_fork();
#endif
    utils::log(LOG_INFO,
      "accept connection from %s", inet_ntoa(cliaddr.sin_addr));

    int status = connection(sock, connfd, servername);
    (void) status;

#ifndef NO_FORK_LOOP
    exit(status);
  }
  else {
    if (pid == -1)
      utils::log(LOG_ERR, "fork failed: %m");
    close(connfd);
    sigprocmask(SIG_SETMASK, &oldmask, 0);
  }
#endif
}

int server::impl::connection(socket_param const &sock, int connfd,
                             std::string const &servername)
{
  try {
    http_connection conn(sock, connfd, servername);
    conn.serve();
  }
  catch(std::exception &e) {
    utils::log(LOG_ERR, "unexpected exception: %s", e.what());
    return 1;
  }
  catch(...) {
    utils::log(LOG_ERR, "unexpected exception");
    return 1;
  }
  return 0;
}

void http_connection::serve() {
  try {
    while (open()) {
      reset();
      response resp(handle_request());
      request_.clear();
      send(resp);
    }
  }
  catch (utils::http::remote_close&) {
    //std::cout << "%% remote or timeout" << std::endl; // DEBUG
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

response http_connection::handle_request() {
  try {
    std::string method, uri, version;
    boost::tie(method, uri, version) = utils::http::get_request_line(conn);

    utils::log(LOG_INFO, "request: method %s uri %s version %s", method.c_str(),
               uri.c_str(), version.c_str());

    assure_relative_uri(uri);

    if (version == "HTTP/1.0") {
      flags.set(HTTP_1_0_COMPAT);
      open_ = false;
    } else if (version != "HTTP/1.1") {
      return response(505);
    }

    request_.read_headers(conn);

    int ret = set_header_options();
    if (ret != 0)
      return response(ret);

    boost::optional<std::string> host_header = request_.get_header("host");
    if (!host_header)
      throw utils::http::bad_format();

    host const *h = sock.get_host(host_header.get());
    if (!h)
      return response(404);

    context &global = h->get_context();

    keywords kw;

    det::any_path path_id;
    det::responder_base *responder;
    context *local;
    global.find_responder(uri, path_id, responder, local, kw);

    if (!responder)
      return response(404);

    kw.set_request_data(request_);

    time_t now;
    std::time(&now);

    time_t last_modified = responder->x_last_modified(path_id, now);
    if (last_modified != time_t(-1) && last_modified > now)
      last_modified = now;
    std::string etag = responder->x_etag(path_id);

    int mod_code = handle_modification_tags(
          last_modified == time_t(-1) ? now : last_modified,
          etag,
          method);

    response out(response::empty_tag());

    if (!mod_code) {
      method_handler_map::const_iterator m = method_handlers.find(method);
      if (m == method_handlers.end())
        return response(501);
      m->second(this, responder, path_id, kw, request_).move(out);
    } else {
      response(mod_code).move(out);
    }

    out.set_header("Date", utils::http::datetime_string(now));
    if (last_modified != time_t(-1))
      out.set_header(
          "Last-modified",
          utils::http::datetime_string(last_modified));
    if (!etag.empty())
      out.set_header("ETag", etag);

    return out;
  } catch (utils::http::bad_format &) {
    return response(400);
  }
}

int http_connection::handle_modification_tags(
  time_t last_modified, std::string const &etag, std::string const &method)
{
  boost::optional<std::string> el;
  el = request_.get_header("if-modified-since");
  if (el) {
    time_t if_modified_since = utils::http::datetime_value(el.get());
    if (if_modified_since >= last_modified)
      return 304;
  }
  el = request_.get_header("if-unmodified-since");
  if (el) {
    time_t if_unmodified_since = utils::http::datetime_value(el.get());
    if (if_unmodified_since < last_modified)
      return 412;
  }
  el = request_.get_header("if-match");
  if (el) {
    if (el.get() == "*") {
      if (etag.empty())
        return 412;
    } else {
      std::vector<std::string> if_match;
      utils::http::parse_list(el.get(), if_match);
      if (std::find(if_match.begin(), if_match.end(), etag) == if_match.end())
        return 412;
    }
  }
  el = request_.get_header("if-none-match");
  if (el) {
    int const fail = (method == "GET" || method == "HEAD") ? 304 : 412;
    if (el.get() == "*") {
      if (!etag.empty())
        return fail;
    } else {
      std::vector<std::string> if_none_match;
      utils::http::parse_list(el.get(), if_none_match);
      if (std::find(if_none_match.begin(), if_none_match.end(), etag) !=
          if_none_match.end())
        return fail;
    }
  }
  el = request_.get_header("if-range");
  if (el) {
    time_t v = utils::http::datetime_value(el.get());
    if (v == time_t(-1)) {
      if (el.get() != etag) 
        request_.erase_header("range");
    } else {
      if (v <= last_modified)
        request_.erase_header("range");
    }
  }
  return 0;
}

response http_connection::handle_get(
  det::responder_base *responder,
  det::any_path const &path_id,
  keywords &kw,
  request const &req)
{
  det::get_base *getter = responder->x_getter();
  if (!getter || !responder->x_exists(path_id, kw))
    return response(404);
  return getter->x_get(path_id, kw, req);
}

response http_connection::handle_head(
  det::responder_base *responder,
  det::any_path const &path_id,
  keywords &kw,
  request const &req)
{
  //TODO: better implementation
  flags.set(NO_ENTITY);
  det::get_base *getter = responder->x_getter();
  if (!getter || !responder->x_exists(path_id, kw))
    return response(404);

  return getter->x_get(path_id, kw, req);
}


response http_connection::handle_post(
  det::responder_base *responder,
  det::any_path const &path_id,
  keywords &kw,
  request const &req)
{
  det::post_base *poster = responder->x_poster();
  if (!poster || !responder->x_exists(path_id, kw))
    return response(404);

  int ret = handle_entity(kw);
  if (ret != 0)
    return response(ret);

  return poster->x_post(path_id, kw, req);
}

response http_connection::handle_put(
  det::responder_base *responder,
  det::any_path const &path_id,
  keywords &kw,
  request const &req)
{
  det::put_base *putter = responder->x_putter();
  if (!putter)
    return response(404);

  int ret = handle_entity(kw);
  if(ret != 0)
    return response(ret);

  return putter->x_put(path_id, kw, req);
}

response http_connection::handle_delete(
  det::responder_base *responder,
  det::any_path const &path_id,
  keywords &kw,
  request const &req)
{
  det::delete__base *deleter = responder->x_deleter();
  if (!deleter || !responder->x_exists(path_id, kw))
    return response(404);
  return deleter->x_delete_(path_id, kw, req);
}

int http_connection::set_header_options() {
  boost::optional<std::string> connect_header =
    request_.get_header("connection");
  if (connect_header) {
    std::vector<std::string> tokens;
    utils::http::parse_list(connect_header.get(), tokens);
    for (std::vector<std::string>::iterator it = tokens.begin();
        it != tokens.end();
        ++it) {
      algo::to_lower(*it);
      if (*it == "close")
        open_ = false;
      if (flags.test(HTTP_1_0_COMPAT))
        request_.erase_header(*it);
    }
  }

  typedef std::multimap<int, std::string> qlist_t;

  qlist_t qlist;
  boost::optional<std::string> accept_encoding =
    request_.get_header("accept-encoding");
  if (accept_encoding)
    utils::http::parse_qlist(accept_encoding.get(), qlist);

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
      else if(i->second == "deflate") {
        encodings.push_back(response::deflate);
        found = true;
      }
      else if(i->second == "identity")
        found = true;
    }
  }

  return 0;
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

int http_connection::handle_entity(keywords &kw) {
  pop_filt_stream fin;

  boost::optional<std::string> content_encoding =
    request_.get_header("content-encoding");

  if (content_encoding) {
    std::vector<std::string> ce;
    utils::http::parse_list(content_encoding.get(), ce);
    for (std::vector<std::string>::iterator it = ce.begin();
        it != ce.end();
        ++it)
    {
      if (algo::iequals(content_encoding.get(), "gzip") ||
           (flags.test(HTTP_1_0_COMPAT) &&
             algo::iequals(content_encoding.get(), "x-gzip")))
        fin.filt().push(io::gzip_decompressor());
      else if (algo::iequals(content_encoding.get(), "bzip2"))
        fin.filt().push(io::bzip2_decompressor());
      else if (algo::iequals(content_encoding.get(), "deflate"))
        fin.filt().push(io::zlib_decompressor());
      else if (!algo::iequals(content_encoding.get(), "identity"))
        return 415;
    }
  }

  boost::optional<std::string> transfer_encoding =
    request_.get_header("transfer-encoding");

  bool chunked = false;

  if(transfer_encoding) {
    std::vector<std::string> te;
    utils::http::parse_list(transfer_encoding.get(), te);
    for (std::vector<std::string>::iterator it = te.begin();
        it != te.end();
        ++it)
    {
      if(algo::iequals(*it, "chunked")) {
        if (it != --te.end())
          return 400;
        chunked = true;
        fin.filt().push(utils::chunked_filter());
      } else if (algo::iequals(*it, "gzip")) {
        fin.filt().push(io::gzip_decompressor());
      } else if (algo::iequals(*it, "deflate")) {
        fin.filt().push(io::zlib_decompressor());
      } else if (!algo::iequals(*it, "identity")) {
        return 501;
      }
    }
  }

  boost::optional<std::string> content_length =
    request_.get_header("content-length");

  if (content_length && !chunked)
    return 411; // Content-length required
  else if (!chunked) {
    std::size_t const length =
      boost::lexical_cast<std::size_t>(content_length.get());
    fin.filt().push(utils::length_filter(length));
  }

  if (!flags.test(HTTP_1_0_COMPAT)) {
    boost::optional<std::string> expect = request_.get_header("expect");
    if (expect) {
      if (!algo::iequals(expect.get(), "100-continue"))
        return 417;
      send(response(100), false);
    }
  }

  fin.filt().push(boost::ref(conn), 0, 0);

  boost::optional<std::string> content_type =
    request_.get_header("content-type");

  std::auto_ptr<std::istream> pstream(new pop_filt_stream(fin.reset()));
  kw.set_entity(
      pstream,
      content_type ? "application/octet-stream" : content_type.get()
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

void http_connection::send(response r, bool entity) {
  //TODO implement partial-GET, entity data from streams

  conn->push_cork();

  io::stream<noflush_writer> out(&conn);

  if (flags.test(HTTP_1_0_COMPAT))
    out << "HTTP/1.0 ";
  else
    out << "HTTP/1.1 ";

  int code = r.get_code();
  if (code == -1)
    code = 200;

  utils::log(LOG_NOTICE, "response: %d", code);

  out << code << " " << response::reason(code) << "\r\n";

  if (!flags.test(HTTP_1_0_COMPAT) && !open_ && code != 100)
    r.add_header_part("Connection", "close");

  r.set_header("Server", servername);

  if (!r.get_type().empty())
    r.set_header("Content-Type", r.get_type());

  if (entity) {
    bool may_chunk = !flags.test(HTTP_1_0_COMPAT);

    response::content_encoding_t enc = r.choose_content_encoding(encodings);

    switch (enc) {
    case response::gzip:
      r.set_header("Content-Encoding", "gzip");
      break;
    case response::bzip2:
      r.set_header("Content-Encoding", "bzip2");
      break;
    case response::deflate:
      r.set_header("Content-Encoding", "deflate");
      break;
    default: break;
    }

    if (!r.chunked(enc)) {
      std::ostringstream length;
      length << r.length(enc);
      r.set_header("Content-Length", length.str());
    } else if (may_chunk) {
      r.set_header("Transfer-Encoding", "chunked");
    }

    r.print_headers(out);
    r.print_entity(out, enc, may_chunk);
  } else {
    r.print_headers(out);
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


