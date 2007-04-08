// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <testsoon.hpp>

using namespace rest::utils::http;

TEST(parse_parametrised) {
  std::string in = "application/x-test ; q=0.7; boundary=\"\\\\\"";
  std::string type;
  std::set<std::string> interests;
  interests.insert("q");
  interests.insert("boundary");
  std::map<std::string, std::string> parameters;
  parse_parametrised(in, type, interests, parameters);
  Equals(type, "application/x-test");
  Equals(parameters.size(), 2U);
  Equals(parameters["q"], "0.7");
  Equals(parameters["boundary"], "\\");
}

TEST(parse_list) {
  std::vector<std::string> elems;
  parse_list("a ,, b1, \tc23,d456 ,e,,", elems);
  Equals(elems.size(), 5U);
  Equals(elems[0], "a");
  Equals(elems[1], "b1");
  Equals(elems[2], "c23");
  Equals(elems[3], "d456");
  Equals(elems[4], "e");
}
