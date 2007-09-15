// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest-utils.hpp"
#include "rest.hpp"
#include <testsoon.hpp>

using namespace rest::utils::http;

TEST(parse_parametrised) {
  std::string in = "application/x-test ; q = 0.7; boundary= \"\\\\\"";
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

TEST(qvalue) {
  Equals(parse_qvalue("0.71"), 710);
}

namespace rest { namespace utils { namespace http {
  void parse_cookie_header(std::string const &in,
                           std::vector<rest::cookie> &cookies);
}}}

TEST_GROUP(parse_cookie_header) {
  XTEST((values, (std::string)
         ("$version = 1, foo = bar; $path = hu; fou = barre") 
         ("$version = 1; foo = bar; $path = hu; fou = barre")
         ("$version = 1; foo = bar; $path = hu, fou = barre")
         ("$version = 1, foo = bar; $path = hu, fou = barre")))
{
    std::string cookie_header = value;
    std::vector<rest::cookie> cookies;

    parse_cookie_header(cookie_header, cookies);

    Equals(cookies.size(), 2U);
    Equals(cookies[0].name, "foo");
    Equals(cookies[0].value, "bar");
    Equals(cookies[0].path, "hu");
    Equals(cookies[1].name, "fou");
    Equals(cookies[1].value, "barre");
  }

  XTEST((values, (std::string)
         ("") (",,") (";") ("   ")))
  {
    std::string cookie_header = value;
    std::vector<rest::cookie> cookies;

    parse_cookie_header(cookie_header, cookies);

    Equals(cookies.empty(), true);
  }

  TEST() {
    std::string cookie_header = " = ";
    std::vector<rest::cookie> cookies;

    parse_cookie_header(cookie_header, cookies);

    Equals(cookies.size(), 1U);
    Equals(cookies[0].name.empty(), true);
    Equals(cookies[0].value.empty(), true);
  }
}
