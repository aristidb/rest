// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/http.hpp"
#include <testsoon.hpp>
#include <boost/tuple/tuple_io.hpp>

using namespace rest::utils::http;

TEST_GROUP(datetime) {

TEST_GROUP(datetime_value) {
  XTEST((2tuples, (std::string, time_t)
         ("Sun, 06 Nov 1994 08:49:37 GMT", 784111777)
         ("Sunday, 06-Nov-94 08:49:37 GMT", 784111777)
         ("Sun Nov  6 08:49:37 1994", 784111777)))
  {
    Equals(value.get<1>(), datetime_value(value.get<0>()));
  }

  XTEST((2tuples, (std::string, std::string)
        ("Sun, 06 Nov 1994 08:49:37 GMT", "Sun, 06 Nov 1994 08:49:37 GMT")
        ("Sunday, 06-Nov-94 08:49:37 GMT", "Sun, 06 Nov 1994 08:49:37 GMT")
        ("Sun Nov  6 08:49:37 1994", "Sun, 06 Nov 1994 08:49:37 GMT")))
  {
    Equals(value.get<1>(), datetime_string(datetime_value(value.get<0>())));
  }
}

}
