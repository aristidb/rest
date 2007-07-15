// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <testsoon.hpp>
#include <sstream>
#include <boost/ref.hpp>
#include <boost/iostreams/filtering_stream.hpp>

using namespace rest::utils;
using namespace boost::iostreams;
namespace io = boost::iostreams;

TEST_GROUP(filters) {
  TEST(length #1) {
    std::stringstream s1;
    s1 << std::string(133, 'x');

    io::filtering_istream fs;
    fs.push(length_filter(100));
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), std::string(100, 'x'));
  }

  TEST(length #2) {
    std::stringstream s1;
    s1 << std::string(40, 'x');

    io::filtering_istream fs;
    fs.push(length_filter(100));
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), std::string(40, 'x'));
  }

  TEST(chunked #1) {
    std::stringstream s1;
    std::size_t n=10;
    std::string s(n, 'x');
    s1 << std::hex << n << "\r\n"
       << s << "\r\n0\r\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), s);
  }

  TEST(chunked #2) {
    std::stringstream s1;
    std::size_t sum = 0;
    for (int i = 0; i < 10; ++i) {
      std::size_t n = 1 + (i % 4);
      sum += n;
      s1 << std::hex << n << "\r\n" << std::string(n, 'x') << "\r\n";
    }
    s1 << "0\r\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), std::string(sum, 'x'));
  }
  
  TEST(chunked #3) {
    std::stringstream s1;
    std::size_t n=10;
    std::string s(n, 'z');
    s1 << std::hex << n << "; very-useless-extension=mega-crap\r\n"
       << s << "\r\n0; useless-extension=crap\r\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), s);
  }

  TEST(chunked invalid) {
    std::stringstream s1;
    s1 << "this is invalid";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), "");
  }

  TEST(chunked invalid: LF only) {
    std::stringstream s1;
    std::size_t n=10;
    std::string s(n, 'x');
    s1 << std::hex << n << "\n"
       << s << "\n0\n";

    io::filtering_istream fs;
    fs.push(chunked_filter());
    fs.push(boost::ref(s1));

    std::stringstream s2;
    s2 << fs.rdbuf();

    Equals(s2.str(), "");
  }

TEST_GROUP(boundary_reader) {

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

TEST() {
  std::istringstream x("a\r\nb\r\nc\r\nboundary");
  filtering_istream s(boundary_filter("\r\nboundary") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals("--" + y.str(), "--a\r\nb\r\nc");
}

XTEST((values, (std::string)("")("ab")("abcd"))) {
  std::istringstream x(value + "\nfoo");
  filtering_istream s;
  s.push(boundary_filter2("\nfoo"));
  s.push(x);
  std::ostringstream y;
  y << s.rdbuf();
  Equals('"' + y.str() + '"', '"' + value + '"');
}

XTEST((values, (std::string)("")("ab")("abcd")("abcdefg"))) {
  std::istringstream x(value);
  filtering_istream s;
  s.push(boundary_filter2("\nfoo"));
  s.push(x);
  std::ostringstream y;
  y << s.rdbuf();
  Equals('"' + y.str() + '"', '"' + value + '"');
}

XTEST((values, (std::string)("")("ab")("abcd"))) {
  std::istringstream x(value);
  filtering_istream s(boundary_filter2("") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals("-" + y.str(), "-");
}

TEST() {
  std::istringstream x("x\nf");
  filtering_istream s(boundary_filter2("\nfoo") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals(y.str(), "x");
}

TEST() {
  std::istringstream x("\nfo");
  filtering_istream s(boundary_filter2("\nfoo") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals("-" + y.str(), "-");
}

TEST() {
  std::istringstream x("a\r\nb\r\nc\r\nboundary");
  filtering_istream s(boundary_filter2("\r\nboundary") | boost::ref(x));
  std::ostringstream y;
  y << s.rdbuf();
  Equals("--" + y.str(), "--a\r\nb\r\nc");
}

}

}
