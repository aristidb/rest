// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <testsoon.hpp>

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
}}
