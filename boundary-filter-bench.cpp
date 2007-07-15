#include "rest-utils.hpp"

#include <sstream>
#include <fstream>
#include <boost/ref.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/iostreams/filtering_stream.hpp>

using namespace rest::utils;
using namespace boost::iostreams;
namespace io = boost::iostreams;

#include <boost/cstdint.hpp>
#include <iostream>
#include <limits>

#include <sys/time.h>

namespace {
  class boundary_filter2 : public boost::iostreams::multichar_input_filter {
  public:
    boundary_filter2(std::string const &boundary)
      : boundary(boundary), buf(new char[boundary.size()]), eof(false),
        pos(boundary.size()) {}
    
    boundary_filter2(boundary_filter2 const &o)
      : boundary(o.boundary), buf(new char[boundary.size()]), eof(false),
        pos(boundary.size()) {}

    template<typename Source>
    std::streamsize read(Source &source, char *outbuf, std::streamsize n) {
      
    }

    template<typename Source>
    void skip_transport_padding(Source &source) {
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

  private:
    std::string boundary;
    boost::scoped_array<char> buf;
    bool eof;
    std::streamsize pos;
  };
  BOOST_IOSTREAMS_PIPABLE(boundary_filter2, 0)

  unsigned long const DEFAULT_TESTS = 10000;

  struct tested {
    boost::uint64_t worstcase;
    boost::uint64_t bestcase;
    boost::uint64_t avg;

    tested()
      : worstcase(0), bestcase(std::numeric_limits<boost::uint64_t>::max()),
        avg(0)
    { }
  };

  template<typename Func>
  tested test(Func f, unsigned long tests = DEFAULT_TESTS) {
    tested t;
    timeval tmpA ={ 0, 0}, tmpB = { 0, 0};

    for(unsigned long i = 0; i < tests; ++i) {
      gettimeofday(&tmpA, 0x0);
      f();
      gettimeofday(&tmpB, 0x0);
      boost::uint64_t testcase = (tmpB.tv_sec - tmpA.tv_sec) * 1000000 +
        (tmpB.tv_usec - tmpA.tv_usec);
      t.avg += testcase;
      if(t.worstcase < testcase)
        t.worstcase = testcase;
      if(t.bestcase > testcase)
        t.bestcase = testcase;
    }

    t.avg /= tests;

    return t;
  }

  void testcase1() {
    std::ifstream x("8zara10.txt");
    filtering_istream s(boundary_filter("\n-------------------------------END!") | boost::ref(x));
    std::ostringstream y;
    y << s.rdbuf();
  }

  void testcase1nofilt() {
    std::ifstream x("8zara10.txt");
    filtering_istream s(boost::ref(x));
    std::ostringstream y;
    y << s.rdbuf();
  }
}

#define TEST(fun) \
  {tested t = test(fun, tests);                                     \
  std::cout << "Tested " << BOOST_PP_STRINGIZE(fun) << ' ' << tests \
            << " times\n"                                           \
            << "Bestcase: " << t.bestcase << "µs\n"     \
            << "Worstcase: " << t.worstcase << "µs\n" \
            << "AVG: " << t.avg << "µs\n";}           \

int main() {
  unsigned long const tests = 100;

  TEST(testcase1)
  TEST(testcase1nofilt)
}
