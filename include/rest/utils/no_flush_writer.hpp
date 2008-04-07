// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_NO_FLUSH_WRITER_HPP
#define REST_UTILS_NO_FLUSH_WRITER_HPP

#include <streambuf>
#include <boost/iostreams/write.hpp>
#include <boost/iostreams/flush.hpp>

namespace rest { namespace utils {

class no_flush_writer {
public:
  typedef char char_type;

  struct category
    :
      boost::iostreams::sink_tag
  {};

  no_flush_writer(std::streambuf *buf) : buf(buf) {}

  std::streamsize write(char const *data, std::streamsize length) {
    return boost::iostreams::write(*buf, data, length);
  }

  void real_flush() {
    boost::iostreams::flush(*buf);
  }

private:
  std::streambuf *buf;
};

}}

#endif
