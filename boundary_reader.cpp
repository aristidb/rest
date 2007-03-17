// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#if 0
#include "rest-utils.hpp"
#include <cstring>
#include <boost/scoped_array.hpp>
#include <istream>
#include <sstream>

#include <testsoon.hpp>
#include <boost/iostreams/stream.hpp>

using namespace rest::utils;
using namespace boost::iostreams;

class boundary_reader::impl {
public:
  impl(std::istream &stream, std::string const &boundary)
  : stream(stream), boundary(boundary), buf(new char[boundary.size()]),
    eof(false), pos(boundary.size()) {}

  std::istream &stream;
  std::string boundary;
  boost::scoped_array<char> buf;
  bool eof;
  std::size_t pos;
};

boundary_reader::boundary_reader(std::istream &s, std::string const &b)
: p(new impl(s, b)) {}

boundary_reader::boundary_reader(boundary_reader const &o)
: p(new impl(o.p->stream, o.p->boundary)) {}

boundary_reader::~boundary_reader() {}

std::streamsize boundary_reader::read(char *outbuf, std::streamsize n) {
  std::streamsize i = 0;
  while (i < n && !p->eof) {
    std::size_t len = p->boundary.size() - p->pos;
    if (p->boundary.compare(0, len, p->buf.get() + p->pos, len) == 0) {
      if (len == p->boundary.size()) {
        p->eof = true;
        break;
      }

      std::memmove(
          p->buf.get(),
          p->buf.get() + p->pos,
          p->boundary.size() - p->pos);

      p->stream.read(p->buf.get() + p->boundary.size() - p->pos, p->pos);
      p->pos = 0;

      if (p->stream.eof()) {
        std::streamsize n = p->stream.gcount();
        if (n <= 0) {
          p->eof = true;
          break;
        }
        p->pos = p->boundary.size() - n;
        std::memmove(p->buf.get() + p->pos, p->buf.get(), n);
      }
      
      if (p->pos == 0) {
        int cmp =
            p->boundary.compare(
              0, p->boundary.size(),
              p->buf.get(), p->boundary.size());
        if (cmp == 0) {
          p->eof = true;
          break;
        }
      }
    }
    outbuf[i++] = p->buf[p->pos++];
  }
  return i;
}

XTEST((values, (std::string)("")("ab")("abcd"))) {
  std::istringstream x(value + "\nfoo");
  stream<boundary_reader> s(x, "\nfoo");
  std::ostringstream y;
  y << s.rdbuf();
  Equals('"' + y.str() + '"', '"' + value + '"');
}

XTEST((values, (std::string)("")("ab")("abcd")("abcdefg"))) {
  std::istringstream x(value);
  stream<boundary_reader> s(x, "\nfoo");
  std::ostringstream y;
  y << s.rdbuf();
  Equals('"' + y.str() + '"', '"' + value + '"');
}

XTEST((values, (std::string)("")("ab")("abcd"))) {
  std::istringstream x(value);
  stream<boundary_reader> s(x, "");
  std::ostringstream y;
  y << s.rdbuf();
  Equals("-" + y.str(), "-");
}

TEST() {
  std::istringstream x("x\nf");
  stream<boundary_reader> s(x, "\nfoo");
  std::ostringstream y;
  y << s.rdbuf();
  Equals(y.str(), "x");
}

TEST() {
  std::istringstream x("\nfo");
  stream<boundary_reader> s(x, "\nfoo");
  std::ostringstream y;
  y << s.rdbuf();
  Equals("-" + y.str(), "-\nfo");
}
#endif
