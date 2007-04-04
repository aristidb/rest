// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include <testsoon.hpp>

TEST() {
  std::string in = "application/x-test ; q=0.7; boundary=\"\\\\\"";
  std::string type;
  std::set<std::string> interests;
  interests.insert("q");
  interests.insert("boundary");
  std::map<std::string, std::string> parameters;
  rest::utils::parse_content_type(in, type, interests, parameters);
  Equals(type, "application/x-test");
  Equals(parameters.size(), 2U);
  Equals(parameters["q"], "0.7");
  Equals(parameters["boundary"], "\\");
}
