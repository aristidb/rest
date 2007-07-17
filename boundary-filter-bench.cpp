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

  void testcase2() {
    std::ifstream x("longrnd.bin");
    filtering_istream s(boundary_filter("\n-------------------------------END!") | boost::ref(x));
    std::ostringstream y;
    y << s.rdbuf();
  }

  void testcase2nofilt() {
    std::ifstream x("longrnd.bin");
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
  TEST(testcase2)
  TEST(testcase2nofilt)
}
