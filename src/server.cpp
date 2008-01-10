// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/server.hpp"
#include "rest/process.hpp"
#include "rest/http_connection.hpp"
#include "rest/host.hpp"
#include "rest/context.hpp"
#include "rest/request.hpp"
#include "rest/config.hpp"
#include "rest/utils/log.hpp"
#include "rest/utils/exceptions.hpp"
#include "rest/utils/socket_device.hpp"
#include <boost/algorithm/string.hpp>
#include <signal.h>
#include <sys/wait.h>

#ifdef APPLE
#include "compat/epoll.h"
#else
#include <sys/epoll.h>
#endif

/*#ifndef NDEBUG //whatever
#define NO_FORK_LOOP
#endif*/

using namespace rest;
namespace det = rest::detail;
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
    int listenfd = network::create_listenfd(*i, listenq);

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
    http_connection conn(sock.hosts(), addr, servername);
    conn.serve(sock, connfd);
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
