// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/server.hpp"
#include "rest/process.hpp"
#include "rest/context.hpp"
#include "rest/config.hpp"
#include "rest/logger.hpp"
#include "rest/scheme.hpp"
#include "rest/signals.hpp"
#include "rest/utils/exceptions.hpp"
#include "rest/utils/socket_device.hpp"
#include <map>
#include <set>
#include <boost/algorithm/string.hpp>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef APPLE
#include "compat/epoll.h"
#else
#include <sys/epoll.h>
#endif

using namespace rest;
namespace det = rest::detail;
namespace algo = boost::algorithm;

class server::impl {
public:
  server *p_ref;

  signals sig;

  std::vector<socket_param> socket_params;
  std::set<int> close_on_fork;

  static int const DEFAULT_LISTENQ;
  static long const DEFAULT_TIMEOUT;
  int listenq;
  long timeout_read;
  long timeout_write;

  int inotify_fd;
  std::map<int /*wd*/, watch_callback_t> inotify_callbacks;

  utils::property_tree const &config;
  logger *log;

  void do_close_on_fork() {
    std::for_each(close_on_fork.begin(), close_on_fork.end(), &::close);
  }

  impl(utils::property_tree const &config, logger *log)
    :
      listenq(utils::get(config, DEFAULT_LISTENQ,
          "connections", "listenq")),
      timeout_read(utils::get(config, DEFAULT_TIMEOUT,
          "connections", "timeout", "read")),
      timeout_write(utils::get(config, DEFAULT_TIMEOUT,
          "connections", "timeout", "write")),
      config(config),
      log(log)
  {
  }

  void configure_signals();

  void read_connections();
  int initialize_sockets();
  void incoming(socket_param const &sock, std::string const &severname);
  int connection(socket_param const &sock, int connfd,
                 rest::network::address const &addr, std::string const &name);

  void initialize_inotify();
  void inotify_event();
};

int const server::impl::DEFAULT_LISTENQ = 5;
long const server::impl::DEFAULT_TIMEOUT = 10;

sockets_container::iterator server::add_socket(socket_param const &s) {
  p->socket_params.push_back(s);
  return --p->socket_params.end();
}

sockets_container &server::sockets() {
  return p->socket_params;
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
      sigset_t empty_mask;
      sigemptyset(&empty_mask);

      int nfds = ::epoll_pwait(epollfd, events, maxevents, -1, &empty_mask);

      if(nfds == -1) {
        if(errno == EINTR)
          return 0;
        throw utils::errno_error("could not run server (epoll_wait)");
      }
      return nfds;
    }
  }
}

void server::impl::read_connections() {
  typedef utils::property_tree::children_iterator children_iterator;
  children_iterator end = config.children_end();
  children_iterator i = utils::get(config, end, "connections");
  if(i == config.children_end()) {
    log->log(logger::warning, "no connections container");
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

    std::string scheme = utils::get(**j, std::string(), "scheme");
    algo::trim(scheme);

    if (scheme.empty())
      throw std::runtime_error("no scheme specified");

    rest::scheme *p_scheme =
      rest::object_registry::get().find<rest::scheme>(scheme);

    if (!p_scheme)
      throw std::runtime_error("invalid scheme");

    boost::any scheme_specific = p_scheme->create_context(log, **j, *p_ref);

    long timeout_read =
      utils::get(**j, this->timeout_read, "timeout", "read");
    long timeout_write =
      utils::get(**j, this->timeout_write, "timeout", "write");

    socket_params.push_back(socket_param(
      service, type, bind, scheme, timeout_read, timeout_write, scheme_specific));
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
    int listenfd = network::create_listenfd(*i, listenq);

    int flags = ::fcntl(listenfd, F_GETFL);
    flags |= O_NONBLOCK;
    ::fcntl(listenfd, F_SETFL, flags);

    close_on_fork.insert(listenfd);

    epolle.data.ptr = &*i;
    if(::epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &epolle) == -1)
      throw utils::errno_error("epoll_ctl (socket)");
  }

#ifndef APPLE
  epolle.data.ptr = 0;
  if (::epoll_ctl(epollfd, EPOLL_CTL_ADD, inotify_fd, &epolle) == -1)
    throw utils::errno_error("epoll_ctl (inotify)");
#endif

  return epollfd;
}

void server::impl::incoming(socket_param const &sock,
                            std::string const &servername)
{
  network::address addr;
  int connfd = network::accept(sock, addr);
  if (connfd < 0) {
    if (errno == EAGAIN || errno == EINTR)
      return;
    log->log(logger::err, "accept-failed", errno);
    log->flush();
    return;
  }

  log->next_sequence_number();

  sigset_t mask, oldmask;
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);

  pid_t pid = ::fork();
  if (pid == 0) {
    try {
      do_close_on_fork();

      log->log(logger::info, "accept-connection", network::ntoa(addr));
      log->flush();

      int const status = connection(sock, connfd, addr, servername);
      _exit(status);
    }
    catch(std::exception &e) {
      log->log(logger::err, "unexpected-exception", e.what());
      log->flush();
      _exit(5);
    }
    catch(...) {
      log->log(logger::err, "unexpected-exception");
      log->flush();
      _exit(6);
    }
  } else {
    if (pid == -1) {
      log->log(logger::err, "fork-failed", errno);
      log->flush();
    }

    close(connfd);
    sigprocmask(SIG_SETMASK, &oldmask, 0);
  }
}

int server::impl::connection(
    socket_param const &sock,
    int connfd,
    network::address const &addr,
    std::string const &servername)
{
  scheme *schm = object_registry::get().find<scheme>(sock.scheme());
  if (!schm) {
    log->log(logger::err, "unknown-scheme", sock.scheme());
    log->flush();
    return 1;
  }
  schm->serve(log, connfd, sock, addr, servername);
  return 0;
}

void server::impl::initialize_inotify() {
#ifndef APPLE
  inotify_fd = inotify_init();

  if (inotify_fd < 0)
    throw utils::errno_error("inotify_init");

  close_on_fork.insert(inotify_fd);
#endif
}

void server::impl::inotify_event() {
#ifndef APPLE
  boost::uint32_t const buf_size = 8192;
  char buf[buf_size];

  ssize_t got = read(inotify_fd, buf, buf_size);

  if (got < 0)
    throw utils::errno_error("read (inotify");

  char *cur = buf;
  char *end = buf + got;

  while (cur < end) {
    struct inotify_event *ev = (struct inotify_event *) cur;
    cur += sizeof(struct inotify_event) + ev->len;

    while (ev->len > 0 && !ev->name[ev->len - 1])
      --ev->len;

    log->log(logger::info, "inotify-ev-file", std::string(ev->name, ev->len));
    log->log(logger::info, "inotify-ev-wd", ev->wd);
    log->log(logger::info, "inotify-ev-mask", ev->mask);
    log->log(logger::info, "inotify-ev-cookie", ev->cookie);

    log->flush();

    watch_callback_t cb = inotify_callbacks[ev->wd];

    if (!cb.empty())
      cb(*ev);
    else
      log->log(logger::warning, "inotify-ev-no-callback", ev->wd);

    log->flush();
  }
#endif
}

void server::impl::configure_signals() {
  sig.ignore(SIGCHLD);
  sig.ignore(SIGPIPE);
  sig.ignore(SIGTSTP);
  sig.ignore(SIGTTIN);
  sig.ignore(SIGTTOU);
  sig.ignore(SIGHUP);
  sig.add(SIGTERM);
  sig.add(SIGINT);
  //sig.add(SIGUSR1);
  sig.block();
}

server::server(utils::property_tree const &conf, logger *log)
: p(new impl(conf, log))
{
  p->p_ref = this;
  p->initialize_inotify();
  p->read_connections();
}

server::~server() { }

void server::set_listen_q(int no) {
  p->listenq = no;
}

void server::serve() {
  p->log->set_sequence_number(0);
  p->log->log(logger::notice, "server-started");
  p->log->flush();

  utils::property_tree &tree = config::get().tree();

  process::maybe_daemonize(p->log, tree);

  p->configure_signals();

  std::string const &servername =
    utils::get(tree, std::string(), "general", "name");
  // shouldn't require a default value (see config::config())
  assert(!servername.empty());

  int epollfd = p->initialize_sockets();

  process::chroot(p->log, tree);
  process::drop_privileges(p->log, tree);

  int const EVENTS_N = 8;

  for (;;) {
    epoll_event events[EVENTS_N];
    int nfds = epoll::wait(epollfd, events, EVENTS_N);

    if (p->sig.is_pending(SIGTERM) || p->sig.is_pending(SIGINT))
      break;

    for(int i = 0; i < nfds; ++i) {
      socket_param *ptr = static_cast<socket_param*>(events[i].data.ptr);
      if (ptr) { // socket
        p->incoming(*ptr, servername);
      } else { // inotify
        p->inotify_event();
      } 
    }
  }

  p->log->log(logger::notice, "server-stopped");
  p->log->flush();
}

int server::watch_file(
  std::string const &file_path,
  inotify_mask_t inotify_mask,
  watch_callback_t const &watch_callback)
{
  int wd = 0;

#ifndef APPLE
  wd = inotify_add_watch(p->inotify_fd, file_path.c_str(), inotify_mask);

  p->log->log(logger::info, "inotify-watch-file", file_path);
  p->log->log(logger::info, "inotify-watch-wd", wd);
  p->log->flush();

  if (wd < 0)
    throw utils::errno_error("inotify_add_watch");

  p->inotify_callbacks.insert(std::make_pair(wd, watch_callback));
#else
  (void)file_path;
  (void)inotify_mask;
  (void)watch_callback;
#endif

  return wd;
}
