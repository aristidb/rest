// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/server.hpp"
#include "rest/process.hpp"
#include "rest/host.hpp"
#include "rest/context.hpp"
#include "rest/request.hpp"
#include "rest/config.hpp"
#include "rest/input_stream.hpp"
#include "rest/utils/http.hpp"
#include "rest/utils/log.hpp"
#include "rest/utils/exceptions.hpp"
#include "rest/utils/chunked_filter.hpp"
#include "rest/utils/length_filter.hpp"
#include "rest/utils/socket_device.hpp"
#include "rest/utils/complete_filtering_stream.hpp"
#include "rest/utils/uri.hpp"

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
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef APPLE
#include "compat/epoll.h"
#else
#include <sys/epoll.h>
#endif

#ifndef NDEBUG //whatever
#define NO_FORK_LOOP
#endif

using namespace rest;
namespace det = rest::detail;
namespace io = boost::iostreams;
namespace algo = boost::algorithm;

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

  static sig_atomic_t restart_flag;
  static void restart_handler(int) {
    restart_flag = 1;
  }

  static void term_handler(int sig) {
    utils::log(LOG_NOTICE, "server is going down (SIG %d)", sig);
    exit(4);
  }

  void read_connections();
  int initialize_sockets();
  void incoming(socket_param const &sock, std::string const &severname);
  int connection(socket_param const &sock, int connfd,
                 rest::network::address const &addr, std::string const &name);
};

sig_atomic_t server::impl::restart_flag = 0;
int const server::impl::DEFAULT_LISTENQ = 5;
long const server::impl::DEFAULT_TIMEOUT = 10;

sockets_container::iterator server::add_socket(socket_param const &s) {
  p->socket_params.push_back(s);
  return --p->socket_params.end();
}

sockets_container &server::sockets() {
  return p->socket_params;
}

void server::set_listen_q(int no) {
  p->listenq = no;
}

server::server(utils::property_tree const &conf) : p(new impl(conf)) { }
server::~server() { }

typedef io::stream_buffer<utils::socket_device> connection_streambuf;

namespace {
  class http_connection {
    socket_param const &sock;
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

    typedef std::vector<std::pair<boost::int64_t, boost::int64_t> > ranges_t;
    ranges_t ranges;

  public:
    http_connection(socket_param const &sock, int connfd,
                    rest::network::address const &addr,
                    std::string const &servername)
    : sock(sock),
      connfd(connfd),
      conn(connfd, sock.timeout_read(), sock.timeout_write()),
      servername(servername),
      open_(true),
      request_(addr)
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

    void serve();

    int handle_modification_tags(
      time_t, std::string const &, std::string const &);

    void handle_caching(
      det::responder_base *, det::any_path const &, response &, time_t, time_t);
    void handle_header_caching(
      det::responder_base *, det::any_path const &, response &, bool &,
      std::string const &);

    void analyze_ranges();

    void tell_allow(response &resp, det::responder_base *responder);

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
    response handle_options(
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
    std::make_pair("DELETE", H(&http_connection::handle_delete)),
    std::make_pair("GET", H(&http_connection::handle_get)),
    std::make_pair("HEAD", H(&http_connection::handle_head)),
    std::make_pair("OPTIONS", H(&http_connection::handle_options)),
    std::make_pair("POST", H(&http_connection::handle_post)),
    std::make_pair("PUT", H(&http_connection::handle_put)),
 };

  static method_handler_map const method_handlers(
    method_handlers_raw,
    method_handlers_raw + 
      sizeof(method_handlers_raw) / sizeof(*method_handlers_raw)
  );
}

namespace {
  namespace epoll {
    int create(int size) {
      int epollfd = ::epoll_create(size);
      if(epollfd == -1)
        throw utils::errno_error("could not start server (epoll_create)");
      network::close_on_exec(epollfd);
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

  int create_listenfd(sockets_container::iterator i, int backlog) {
    addrinfo *res;
    network::getaddrinfo(*i, &res);
    addrinfo *const ressave = res;

    int listenfd;
    do {
      listenfd = network::socket(i->socket_type());

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
               i->socket_type() == network::ip4 ? "IPv4" : "IPv6",
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
    network::socket_type_t type;
    if(algo::istarts_with(type_, "ipv4") ||
       algo::istarts_with(type_, "ip4"))
      type = network::ip4;
    else if(algo::istarts_with(type_, "ipv6") ||
            algo::istarts_with(type_, "ip6"))
      type = network::ip6;
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

  for(sockets_container::iterator i = socket_params.begin();
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

  ::signal(SIGTERM, &impl::term_handler);
  ::siginterrupt(SIGTERM, 0);

  ::signal(SIGCHLD, &impl::sigchld_handler);
  ::siginterrupt(SIGCHLD, 0);

  ::signal(SIGUSR1, &impl::restart_handler);
  ::siginterrupt(SIGUSR1, 0);

  // TODO: ignore SIGPIPE?

  std::string const &servername =
    utils::get(tree, std::string(), "general", "name");
  // shouldn't require a default value (see config::config())
  assert(!servername.empty());

  int epollfd = p->initialize_sockets();

  int const EVENTS_N = 8;

  for(;;) {
    epoll_event events[EVENTS_N];
    int nfds = epoll::wait(epollfd, events, EVENTS_N);
    if (impl::restart_flag)
      process::restart();
    for(int i = 0; i < nfds; ++i) {
      socket_param *ptr = static_cast<socket_param*>(events[i].data.ptr);
      assert(ptr);
      p->incoming(*ptr, servername);
    }
  }
}

void server::impl::incoming(socket_param const &sock,
                            std::string const &servername)
{
  network::address addr;
  int connfd = network::accept(sock, addr);
  if (connfd < 0) {
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
               "accept connection from %s", network::ntoa(addr).c_str());

    int status = connection(sock, connfd, addr, servername);
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
                             network::address const &addr,
                             std::string const &servername)
{
  try {
    http_connection conn(sock, connfd, addr, servername);
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
      if (!resp.check_ranges(ranges)) {
        // Invalid range - send appropriate response
        boost::int64_t length = resp.length(rest::response::identity);
        response(416).move(resp);
        std::ostringstream range;
        range << "bytes */";
        if (length < 0)
          range << length;
        else
          range << '*';
        resp.set_header("Content-Range", range.str()); 
        ranges_t().swap(ranges);
      }
      if (resp.is_nil())
        request_.get_host().make_standard_response(resp);
      request_.clear();
      send(resp);
      ranges_t().swap(ranges);
    }
  }
  catch (utils::http::remote_close&) {
  }
}

response http_connection::handle_request() {
  try {
    std::string method, uri, version;
    boost::tie(method, uri, version) = utils::http::get_request_line(conn);

    utils::log(LOG_INFO, "request: method %s uri %s version %s", method.c_str(),
               uri.c_str(), version.c_str());

    utils::uri::make_relative(uri);
    request_.set_uri(uri);

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

    boost::optional<std::string> host_header = request_.get_header("Host");
    if (!host_header)
      throw utils::http::bad_format();

    host const *h = sock.get_host(host_header.get());
    if (!h)
      return response(404);

    request_.set_host(*h);

    context &global = h->get_context();

    keywords kw;

    det::any_path path_id;
    det::responder_base *responder;
    context *local;
    global.find_responder(uri, path_id, responder, local, kw);

    if (!responder && !(method == "OPTIONS" && uri == "*"))
      return response(404);

    kw.set_request_data(request_);

    time_t now;
    std::time(&now);

    time_t last_modified = time_t(-1);
    if (responder)
      last_modified = responder->x_last_modified(path_id, now);
    if (last_modified != time_t(-1) && last_modified > now)
      last_modified = now;
    std::string etag;
    if (responder)
      etag = responder->x_etag(path_id);
    time_t expires = time_t(-1);
    if (responder)
      expires = responder->x_expires(path_id, now);

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

    tell_allow(out, responder);

    out.set_header("Date", utils::http::datetime_string(now));
    if (last_modified != time_t(-1))
      out.set_header(
          "Last-Modified",
          utils::http::datetime_string(last_modified));
    if (!etag.empty())
      out.set_header("ETag", etag);

    if (responder)
      handle_caching(responder, path_id, out, now, expires);

    if (method == "GET" || method == "HEAD") {
      int code = out.get_code();
      if (code == -1 || (code >= 200 && code <= 299)) {
        if (out.length(response::identity) >= 0)
          out.set_header("Accept-Ranges", "bytes");
        else
          out.set_header("Accept-Ranges", "none");
      }
    }

    return out;
  } catch (utils::http::bad_format &) {
    return response(400);
  }
}

int http_connection::handle_modification_tags(
  time_t last_modified, std::string const &etag, std::string const &method)
{
  int code = 0;
  boost::optional<std::string> el;
  el = request_.get_header("If-Modified-Since");
  if (el) {
    time_t if_modified_since = utils::http::datetime_value(el.get());
    if (if_modified_since < last_modified)
      return 0;
    code = 304;
  }
  el = request_.get_header("If-Unmodified-Since");
  if (el) {
    time_t if_unmodified_since = utils::http::datetime_value(el.get());
    if (if_unmodified_since >= last_modified)
      return 0;
    code = 412;
  }
  el = request_.get_header("If-Match");
  if (el) {
    if (el.get() == "*") {
      if (!etag.empty())
        return 0;
    } else {
      std::vector<std::string> if_match;
      utils::http::parse_list(el.get(), if_match);
      if (std::find(if_match.begin(), if_match.end(), etag) != if_match.end())
        return 0;
    }
    code = 412;
  }
  el = request_.get_header("If-None-Match");
  if (el) {
    if (el.get() == "*") {
      if (etag.empty())
        return 0;
    } else {
      std::vector<std::string> if_none_match;
      utils::http::parse_list(el.get(), if_none_match);
      if (std::find(if_none_match.begin(), if_none_match.end(), etag) ==
          if_none_match.end())
        return 0;
    }
    if (code != 412)
      code = (method == "GET" || method == "HEAD") ? 304 : 412;
  }
  el = request_.get_header("If-Range");
  if (el) {
    time_t v = utils::http::datetime_value(el.get());
    if (v == time_t(-1)) {
      if (el.get() != etag) 
        request_.erase_header("Range");
    } else {
      if (v <= last_modified)
        request_.erase_header("Range");
    }
  }
  return code;
}

void http_connection::handle_caching(
  det::responder_base *responder,
  det::any_path const &path_id,
  response &resp,
  time_t now,
  time_t expires)
{
  bool cripple_expires = false;
  cache::flags general = responder->x_cache(path_id);

  if (general & cache::private_) {
    cripple_expires = true;
    resp.add_header_part("Cache-Control", "private");
  } else {
    resp.add_header_part("Cache-Control", "public");
  }

  if (general & cache::no_cache) {
    resp.add_header_part("Cache-Control", "no-cache");
    resp.add_header_part("Pragma", "no-cache");
  }

  if (general & cache::no_store)
    resp.add_header_part("Cache-Control", "no-store");

  if (general & cache::no_transform)
    resp.add_header_part("Cache-Control", "no-transform");

  resp.list_headers(boost::bind(
      &http_connection::handle_header_caching,
      this,
      responder,
      boost::cref(path_id),
      boost::ref(resp),
      boost::ref(cripple_expires),
      _1));

  if (expires != time_t(-1)) {
    std::ostringstream s_max_age;
    time_t max_age = (expires > now) ? (expires - now) : 0;
    s_max_age << "max-age=" << max_age;
    resp.add_header_part("Cache-Control", s_max_age.str());

    if (!cripple_expires)
      resp.set_header("Expires", utils::http::datetime_string(expires));
  } else if (!cripple_expires) {
    resp.set_header("Expires", "");
  }

  if (cripple_expires)
    resp.set_header("Expires", "0");
}

void http_connection::handle_header_caching(
  det::responder_base *responder,
  det::any_path const &path_id,
  response &resp,
  bool &cripple_expires,
  std::string const &header)
{
  cache::flags flags = responder->x_cache(path_id, header);
  if (flags & cache::no_cache) {
    cripple_expires = true;
    std::string x;
    x += "no-cache="; x += '"'; x += header; x += '"';
    resp.add_header_part("Cache-Control",  x);
  }
  if (flags & cache::private_) {
    cripple_expires = true;
    std::string x;
    x += "private="; x += '"'; x += header; x += '"';
    resp.add_header_part("Cache-Control",  x);
  }
}

response http_connection::handle_options(
  det::responder_base *,
  det::any_path const &,
  keywords &,
  request const &)
{
  response resp(200);
  return resp;
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
  response r(getter->x_get(path_id, kw, req));
  int code = r.get_code();
  if ((code == -1 || (code >= 200 && code <= 299)) && !r.is_nil())
    analyze_ranges();
  return r;
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

void http_connection::tell_allow(response &resp, det::responder_base *responder)
{
  resp.add_header_part("Allow", "OPTIONS");
  if (!responder) {
    resp.add_header_part("Allow", "DELETE");
    resp.add_header_part("Allow", "GET");
    resp.add_header_part("Allow", "HEAD");
    resp.add_header_part("Allow", "POST");
    resp.add_header_part("Allow", "PUT");
  } else {
    if (responder->x_getter()) {
      resp.add_header_part("Allow", "GET");
      resp.add_header_part("Allow", "HEAD");
    }
    if (responder->x_poster()) {
      resp.add_header_part("Allow", "POST");     
    }
    if (responder->x_deleter()) {
      resp.add_header_part("Allow", "DELETE");
    }
    if (responder->x_putter()) {
      resp.add_header_part("Allow", "PUT");
    }
  }
}

int http_connection::set_header_options() {
  boost::optional<std::string> connect_header =
    request_.get_header("Connection");
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
    request_.get_header("Accept-Encoding");
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

void http_connection::analyze_ranges() {
  boost::optional<std::string> range_ = request_.get_header("Range");
  if (!range_)
    return;
  std::string const &range = range_.get();

  typedef std::vector<std::string> s_vect;
  s_vect seq;

  rest::utils::http::parse_list(range, seq, '=');
  if (seq.size() != 2 || !algo::iequals(seq[0], "bytes"))
    return;

  std::string data;
  seq[1].swap(data);
  seq.clear();

  rest::utils::http::parse_list(data, seq, ',');
  if (seq.empty())
    return;

  for (s_vect::iterator it = seq.begin(); it != seq.end(); ++it) {
    std::vector<std::string> x;
    rest::utils::http::parse_list(*it, x, '-', false);
    if (x.size() != 2)
      goto bad;
    std::pair<boost::int64_t, boost::int64_t> v(-1, -1);
    try {
      if (!x[0].empty()) {
        v.first = boost::lexical_cast<long>(x[0]);
        if (v.first < 0)
          goto bad;
      }
      if (!x[1].empty()) {
        v.second = boost::lexical_cast<long>(x[1]);
        if (v.second < 0)
          goto bad;
      }
    } catch (boost::bad_lexical_cast &) {
      goto bad;
    }
    if (v.first == -1 && v.second == -1)
      goto bad;
    if (v.first != -1 && v.second != -1 && v.first > v.second)
      goto bad;
      
    ranges.push_back(v);
  }

  return;

  bad:
    ranges_t().swap(ranges);
    return;
}

int http_connection::handle_entity(keywords &kw) {
  std::auto_ptr<io::filtering_streambuf<io::input> > fin
    (new io::filtering_streambuf<io::input>);

  boost::optional<std::string> content_encoding =
    request_.get_header("Content-Encoding");

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
        fin->push(io::gzip_decompressor());
      else if (algo::iequals(content_encoding.get(), "bzip2"))
        fin->push(io::bzip2_decompressor());
      else if (algo::iequals(content_encoding.get(), "deflate"))
        fin->push(io::zlib_decompressor());
      else if (!algo::iequals(content_encoding.get(), "identity"))
        return 415;
    }
  }

  boost::optional<std::string> transfer_encoding =
    request_.get_header("Transfer-Encoding");

  bool chunked = false;

  if (transfer_encoding) {
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
        fin->push(utils::chunked_filter());
      } else if (algo::iequals(*it, "gzip")) {
        fin->push(io::gzip_decompressor());
      } else if (algo::iequals(*it, "deflate")) {
        fin->push(io::zlib_decompressor());
      } else if (!algo::iequals(*it, "identity")) {
        return 501;
      }
    }
  }

  boost::optional<std::string> content_length =
    request_.get_header("Content-Length");

  if (!content_length && !chunked) {
    return 411; // Content-length required
  } else if (!chunked) {
    boost::uint64_t const length =
      boost::lexical_cast<boost::uint64_t>(content_length.get());
    
    fin->push(utils::length_filter(length));
  }

  if (!flags.test(HTTP_1_0_COMPAT)) {
    boost::optional<std::string> expect = request_.get_header("expect");
    if (expect) {
      if (!algo::iequals(expect.get(), "100-continue"))
        return 417;
      send(response(100), false);
    }
  }

  fin->push(boost::ref(conn), 0, 0);

  boost::optional<std::string> content_type =
    request_.get_header("Content-Type");

  input_stream pstream(new utils::complete_filtering_stream(fin.release()));
  kw.set_entity(
      pstream,
      !content_type ? "application/octet-stream" : content_type.get()
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

  if (code >= 400)
    open_ = false;

  if (!flags.test(HTTP_1_0_COMPAT) && !open_ && code != 100)
    r.add_header_part("Connection", "close");

  r.set_header("Server", servername);

  if (entity) {
    bool may_chunk = !flags.test(HTTP_1_0_COMPAT);

    response::content_encoding_t enc =
      r.choose_content_encoding(encodings, !ranges.empty());

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

    if (ranges.empty() && !r.chunked(enc)) {
      std::ostringstream length;
      length << r.length(enc);
      r.set_header("Content-Length", length.str());
    } else if (may_chunk) {
      r.set_header("Transfer-Encoding", "chunked");
    }

    r.print_headers(out);
    r.print_entity(out, enc, may_chunk, ranges);
  } else {
    r.print_headers(out);
  }

  io::flush(out);
  conn->loosen_cork();
  out->real_flush();
  conn->pull_cork();
}

