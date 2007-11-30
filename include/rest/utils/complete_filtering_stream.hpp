// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_COMPLETE_FILTERING_STREAM_HPP
#define REST_UTILS_COMPLETE_FILTERING_STREAM_HPP

#include <istream>
#include <boost/iostreams/filtering_streambuf.hpp>

namespace rest { namespace utils {

class complete_filtering_stream : public std::istream {
public:
  typedef boost::iostreams::filtering_streambuf<boost::iostreams::input> buf_t;

  explicit complete_filtering_stream(buf_t *buf = new buf_t) : buf(buf) {
    rdbuf(buf);
  }

  ~complete_filtering_stream() {
    if (buf) {
      if (buf->is_complete()) {
        ignore(std::numeric_limits<int>::max());
        buf->pop();
      }
      delete buf;
    }
  }

  buf_t *operator->() { return buf; }

  buf_t *release() {
    buf_t *ptr = buf;
    buf = 0;
    return ptr;
  }

private:
  buf_t *buf;
};

}}

#endif
