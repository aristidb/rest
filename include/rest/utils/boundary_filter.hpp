// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS_BOUNDARY_FILTER_HPP
#define REST_UTILS_BOUNDARY_FILTER_HPP

#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/read.hpp>
#include <string>
#include <vector>

namespace rest { namespace utils {

class boundary_filter : public boost::iostreams::multichar_input_filter {
public:
  boundary_filter(std::string const &boundary)
  : boundary(boundary),
    buf(new char[boundary.size()]),
    eof(false),
    pos(boundary.size()),
    boundary_pos(pos)
  {
    kmp_init();
  }

  boundary_filter(boundary_filter const &o)
  : boundary(o.boundary),
    buf(new char[boundary.size()]),
    eof(false),
    pos(boundary.size()),
    boundary_pos(pos)
  {
    kmp_init();
  }

  ~boundary_filter() {
    delete [] buf;
  }

private:
  struct eof_event {};

public:
  template<typename Source>
  std::streamsize read(Source & __restrict, char * __restrict, std::streamsize);

private:
  template<typename Source>
  std::size_t update(Source & __restrict);

  void kmp_init();
  std::size_t check_boundary();

  template<typename Source>
  void skip_transport_padding(Source & __restrict);

private:
  std::string const boundary;
  char * __restrict buf;
  bool eof;
  std::size_t pos;
  std::size_t boundary_pos;
  std::vector<int> kmp_next;
};

template<typename Source>
std::streamsize boundary_filter::read(
    Source & __restrict source,
    char * __restrict outbuf,
    std::streamsize outbuf_size_)
{
  if (eof)
    return -1;
  if (outbuf_size_ <= 0)
    return 0;
  std::size_t outbuf_size = std::size_t(outbuf_size_);
  std::size_t outbuf_pos = 0;
  try {
    while (outbuf_pos < outbuf_size) {
      std::size_t fresh_bytes = update(source) - pos;
      std::size_t read_bytes = std::min(fresh_bytes, outbuf_size - outbuf_pos);
      memcpy(outbuf + outbuf_pos, buf + pos, read_bytes);
      outbuf_pos += read_bytes;
      pos += read_bytes;
    }
  } catch (eof_event&) {
    skip_transport_padding(source);
    eof = true;
  }
  return outbuf_pos ? outbuf_pos : -1;
}

template<typename Source>
std::size_t boundary_filter::update(Source & __restrict source) {
  namespace io = boost::iostreams;

  bool end_of_input = false;

  while (boundary_pos == pos) {
    if (end_of_input || pos == 0)
      throw eof_event();

    memmove(buf, buf + pos, boundary.size() - pos);

    do {
      std::streamsize input_size =
        io::read(source, buf + boundary.size() - pos, pos);

      if (input_size < 0)
        break;

      pos -= input_size;
    } while (pos > 0);

    if (pos != 0) {
      end_of_input = true;
      memmove(buf + pos, buf, boundary.size() - pos);
    }

    boundary_pos = check_boundary();
  }

  return boundary_pos;
}

inline void boundary_filter::kmp_init() {
  kmp_next.resize(boundary.size() + 1);
  std::size_t i = 0;
  int j = -1;
  kmp_next[0] = -1;
  while (i < boundary.size()) {
    while (j >= 0 && boundary[j] != boundary[i])
      j = kmp_next[j];
    ++i;
    ++j;
    kmp_next[i] = j;
  }
}

inline std::size_t boundary_filter::check_boundary() {
  std::size_t i = pos;
  int j = 0, j2;
  std::size_t x = i;
  while (i < boundary.size()) {
    j2 = j;
    while (j >= 0 && buf[i] != boundary[j])
      j = kmp_next[j];
    ++i;
    ++j;
    if (j <= 0)
      x = i;
    else if (j <= j2)
      x = i - 1;
  }
  return x;
}

template<typename Source>
void boundary_filter::skip_transport_padding(Source & __restrict source) {
  int ch;
  // skip LWS
  while ((ch = boost::iostreams::get(source)) == ' ' && ch == '\t')
    ;
  if (ch == EOF)
    return;
  // "--" indicates end-of-streams
  if (ch == '-' && boost::iostreams::get(source) == '-') {
    // soak up all the content, it is to be ignored
    while (boost::iostreams::get(source) != EOF)
      ;
    return;
  }
  // expect CRLF
  boost::iostreams::putback(source, ch);
  if ((ch = boost::iostreams::get(source)) != '\r') {
    if (ch == EOF)
      return;
    boost::iostreams::putback(source, ch);
  }
  if ((ch = boost::iostreams::get(source)) != '\n') {
    if (ch == EOF)
      return;
    boost::iostreams::putback(source, ch);
  }
}

}}

#endif
