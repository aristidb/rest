// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_SOCKET_DEVICE_HPP
#define REST_UTILS_SOCKET_DEVICE_HPP

#include <boost/iostreams/concepts.hpp>
#include <boost/shared_ptr.hpp>
#include <iosfwd>

namespace rest { namespace utils {

struct socket_device {
  typedef char char_type;

  struct category
    :
      boost::iostreams::bidirectional_device_tag,
      boost::iostreams::closable_tag
  {};

  socket_device(int fd, long timeout_rd, long timeout_wr);
  ~socket_device();

  void close(std::ios_base::open_mode);

  void push_cork();
  void loosen_cork();
  void pull_cork();

  std::streamsize read(char *, std::streamsize);
  std::streamsize write(char const *, std::streamsize);

private:
  class impl;
  boost::shared_ptr<impl> p;
};

}}

#endif
