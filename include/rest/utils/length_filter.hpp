// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_LENGTH_FILTER_HPP
#define REST_UTILS_LENGTH_FILTER_HPP

#include <iosfwd>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/cstdint.hpp>

namespace rest { namespace utils {

class length_filter : public boost::iostreams::multichar_input_filter {
public:
  length_filter(boost::uint64_t length) : length(length) {}

  template<typename Source>
  std::streamsize read(
      Source & __restrict source,
      char * __restrict outbuf,
      std::streamsize n)
  {
    if (n <= 0)
      return n;

    namespace io = boost::iostreams;

    std::streamsize c;
    if (boost::uint64_t(n) <= length)
      c = io::read(source, outbuf, n);
    else
      c = io::read(source, outbuf, length);
    if (c < 0)
      return -1;
    length -= boost::uint64_t(c);
    return c;
  }

private:
  boost::uint64_t length;
};

}}

#endif
