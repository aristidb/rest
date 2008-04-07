// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_CHUNKED_FILTER_HPP
#define REST_UTILS_CHUNKED_FILTER_HPP

#include <cassert>
#include <iosfwd>
#include <ios>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/write.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/iostreams/flush.hpp>
#include <boost/format.hpp>
#include <boost/tuple/tuple.hpp>

namespace rest { namespace utils {

inline boost::tuple<bool,int> hex2int(int ascii) {
  if(std::isdigit(ascii))
    return boost::make_tuple(true, ascii - '0');
  else if(ascii >= 'a' && ascii <= 'f')
    return boost::make_tuple(true, ascii - 'a' + 0xa);
  else if(ascii >= 'A' && ascii <= 'F')
    return boost::make_tuple(true, ascii - 'A' + 0xA);
  else
    return boost::make_tuple(false, 0);
}

struct chunked_error : std::ios_base::failure {
  chunked_error() : std::ios_base::failure("chunked error") {}
};

class chunked_filter {
public:
  typedef char char_type;

  struct category
    :
      boost::iostreams::filter_tag,
      boost::iostreams::multichar_tag,
      boost::iostreams::dual_use,
      boost::iostreams::closable_tag
  {};

  chunked_filter() : pending(0) { }

  template<typename Sink>
  std::streamsize write(
      Sink & __restrict snk,
      char const * __restrict s,
      std::streamsize n)
  {
    namespace io = boost::iostreams;

    assert(n >= 0);

    if (n == 0)
      return 0;

    boost::format length("%1$x");
    length % n;

    std::streamsize ret = io::write(snk, length.str().c_str(),
                                    length.str().length());
    if(ret < 0)
      return ret;
    if( (ret = io::write(snk, "\r\n", 2)) < 0 )
      return ret;
    if( (ret = io::write(snk, s, n)) < 0 )
      return ret;
    if( (ret = io::write(snk, "\r\n", 2)) < 0)
      return ret;

    return n;
  }

  template<typename Device>
  void close(Device &d, std::ios_base::open_mode mode) {
    namespace io = boost::iostreams;
    if (mode & std::ios_base::out) {
      io::write(d, "0\r\n\r\n", 5);
      io::flush(d);
    }
  }


  template<typename Source>
  std::streamsize read(
      Source & __restrict source,
      char * __restrict outbuf,
      std::streamsize n)
  {
    namespace io = boost::iostreams;

    if (pending == -1)
      return -1;
    else if (pending == 0) {
      int c = io::get(source);
      if(c == Source::traits_type::eof())
        return -1;
      else if(c == '\r') {
        c = io::get(source);
        if(c != '\n')
         return -1;
        c = io::get(source);
        if(c == Source::traits_type::eof())
          return -1;
      }
      for(int digit_count = 0; digit_count < 16; ++digit_count) {
        boost::tuple<bool, int> value = hex2int(c);
        if(value.get<0>()) {
          pending *= 0x10;
          pending += value.get<1>();
        }
        else
          break;
        c = io::get(source);
        if(c == Source::traits_type::eof())
          return -1;
      }
      bool cr = false;
      for(;;) {
        if(c == '\r')
          cr = true;
        else if(cr && c == '\n')
          break;
        else if(c == Source::traits_type::eof())
          return -1;
        c = io::get(source);
      }
      if(pending == 0) {
        pending = -1;
        return -1;
      }
    }
    std::streamsize c;
    if (n <= pending)
      c = io::read(source, outbuf, n);
    else
      c = io::read(source, outbuf, pending);
    if (c == -1)
      throw chunked_error();
    pending -= c;
    return c;
  }

private:
  std::streamsize pending;
};

}}

#endif
