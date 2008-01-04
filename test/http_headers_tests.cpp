// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/http.hpp"
#include "rest/cookie.hpp"
#include <sstream>
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

TEST_GROUP(input) {
  TEST_GROUP(get_until) {
    TEST(empty) {
      std::istringstream data(" ??");
      std::string x;
      get_until(' ', data, x);
      Check(x.empty());
    }

    TEST(nospace) {
      std::istringstream data("xy");
      std::string x;
      try {
        get_until(' ', data, x);
      } catch (remote_close&) {
        return;
      } catch (...) {
      }
      Check(!"should have thrown remote_close exception");
    }

    TEST(space) {
      std::istringstream data("ab c");
      std::string x;
      get_until(' ', data, x);
      Equals(x, "ab");
    }

    TEST(newline) {
      std::istringstream data("ab\nc");
      std::string x;
      try {
        get_until(' ', data, x);
      } catch (bad_format&) {
        return;
      } catch (...) {
      }
      Check(!"should have thrown bad_format exception");
    }

    TEST(allowed newline) {
      std::istringstream data("ab\ncd$44");
      std::string x;
      get_until('$', data, x, true);
      Equals(x, "ab\ncd");
    }

    TEST(limited length VALID) {
      std::istringstream data("ab ");
      std::string x;
      get_until(' ', data, x, true, 5);
      Equals(x, "ab");
    }

    TEST(limited length INVALID) {
      std::istringstream data("abcdefg ");
      std::string x;
      try {
        get_until(' ', data, x, true, 5);
      } catch (bad_format&) {
        return;
      } catch (...) {
      }
      Check(!"should have thrown bad_format exception");
    }
  }
}
