// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <cstring>
#include <boost/scoped_array.hpp>
#include <istream>

using namespace rest::util;
using namespace boost::iostreams;

class boundary_reader::impl {
public:
  impl(std::istream &stream, std::string const &boundary)
  : stream(stream), boundary(boundary), buf(new char[boundary.size()]),
    eof(false), pos(0) {}

  std::istream &stream;
  std::string boundary;
  boost::scoped_array<char> buf;
  bool eof;
  std::size_t pos;

  void underflow() {
    std::memmove(buf.get(), buf.get() + pos, boundary.size() - pos);
    stream.get(buf.get(), pos);
    pos = 0;
    if (boundary.compare(0, boundary.size(), buf.get(), boundary.size()) == 0)
      eof = true;
    if (stream.eof())
      eof = true;
  }

  void get(char &ch) {
    std::size_t len = boundary.size() - pos;
    if (boundary.compare(0, len, buf.get(), len) == 0)
      underflow();
    if (eof)
      return;
    ch = buf[pos];
  }
};

boundary_reader::boundary_reader(std::istream &s, std::string const &b)
: p(new impl(s, b)) {}

boundary_reader::~boundary_reader() {}

std::streamsize boundary_reader::read(char *buf, std::streamsize n) {
  std::size_t i = 0;
  while (n > 0 && !p->eof) {
    p->get(buf[i]);
    if (p->eof)
      break;
    ++i;
  }
  return n - i;
}
