// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/http.hpp"
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_smallint.hpp>
#include <boost/random/variate_generator.hpp>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

std::string
rest::utils::http::random_boundary(std::size_t length) {
  boost::rand48 rnd(boost::uint64_t(getpid() + time(0) + clock()));
  boost::uniform_smallint<int> dist(0, 61);
  boost::variate_generator<boost::rand48 &, boost::uniform_smallint<int> >
      gen(rnd, dist);
  static char const letters[63] = 
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  std::string out;
  out.reserve(length);
  for (std::size_t i = 0; i < length; ++i)
    out += letters[gen()];
  return out;
}
