// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/ref.hpp>
#include <bitset>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <netdb.h>

using namespace rest;
using namespace boost::multi_index;
namespace io = boost::iostreams;

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
  enum {
    NO_ENTITY = 1,
    HTTP_1_0_COMPAT = 2,
    ACCEPT_GZIP = 4,
    ACCEPT_BZIP2 = 8
  };
  state_flags flags;

  static const int LISTENQ = 5; // TODO: configurable

  impl(short port) : port(port) {}

  response handle_request();
  void send(response const &);
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
