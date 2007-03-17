// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <testsoon.hpp>
#include <sstream>
#include <boost/ref.hpp>
#include <boost/iostreams/filtering_stream.hpp>

using namespace rest::utils;
using namespace boost::iostreams;

XTEST((values, (std::string)("")("ab")("abcd"))) {
  std::istringstream x(value + "\nfoo");
  filtering_istream s;
  s.push(boundary_filter("\nfoo"));
  s.push(x);
  std::ostringstream y;
  y << s.rdbuf();
  Equals('"' + y.str() + '"', '"' + value + '"');
}

XTEST((values, (std::string)("")("ab")("abcd")("abcdefg"))) {
  std::istringstream x(value);
  filtering_istream s;
  s.push(boundary_filter("\nfoo"));
  s.push(x);
  std::ostringstream y;
  y << s.rdbuf();
  Equals('"' + y.str() + '"', '"' + value + '"');
}

XTEST((values, (std::string)("")("ab")("abcd"))) {
  std::istringstream x(value);
  filtering_istream s(boundary_filter("") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals("-" + y.str(), "-");
}

TEST() {
  std::istringstream x("x\nf");
  filtering_istream s(boundary_filter("\nfoo") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals(y.str(), "x");
}

TEST() {
  std::istringstream x("\nfo");
  filtering_istream s(boundary_filter("\nfoo") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals("-" + y.str(), "-");
}

