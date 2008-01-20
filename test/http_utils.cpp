// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/http.hpp"
#include "rest/cookie.hpp"
#include <sstream>
#include <cctype>
#include <testsoon.hpp>
#include <boost/tuple/tuple_io.hpp>

using namespace rest::utils::http;

TEST_GROUP(spht) {
  XTEST((name, "isspht valid")(values, (char)(' ')('\t'))) {
    Check(isspht(value));
  }

  struct nospht_generator {
    typedef char value_type;
    typedef char const &const_reference;

    struct iterator {
      unsigned char ch;

      iterator(unsigned char ch) : ch(ch) {
        check();
      }

      bool operator!=(iterator const &rhs) {
        return ch != rhs.ch;
      }

      value_type operator*() { return ch; }

      iterator &operator++() {
        ++ch;
        check();
        return *this;
      }

      void check() {
        if (ch == ' ' || ch == '\t')
          ++ch;
      }
    };

    iterator begin() { return iterator(1); }
    iterator end() { return iterator(0); }
  };

  XTEST((name, "isspht invalid")(generator, (nospht_generator))) {
    Check(!isspht(value));
  }
}

TEST_GROUP(expect) {
  TEST(expect + get) {
    std::istringstream in("xy");
    Check(expect(in, 'x'));
    std::string y;
    Check(in >> y);
    Equals(y, "y");
  }
}

TEST_GROUP(datetime) {
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

TEST_GROUP(random_boundary) {
  XTEST((name, "random_boundary")(values, (std::size_t)(1)(10)(20))) {
    std::string x = random_boundary(value);
    Equals(x.size(), value);
    for (std::string::const_iterator it = x.begin(); it != x.end(); ++it)
      Check(std::isalnum(*it));
  }
}

TEST_GROUP(read_headers) {
  TEST(empty) {
    std::istringstream in;
    std::map<std::string, std::string> x;
    try {
      read_headers(in, x);
    } catch (remote_close&) {
      Check(x.empty());
      return;
    }
    Check(!"incomplete request");
  }

  TEST(empty 2) {
    std::istringstream in("\r\n\r\n");
    std::map<std::string, std::string> x;
    read_headers(in, x);
    Check(x.empty());
  }

  TEST(incomplete 1) {
    std::istringstream in("foo");
    std::map<std::string, std::string> x;
    try {
      read_headers(in, x);
    } catch (remote_close&) {
      Check(x.empty());
      return;
    }
    Check(!"incomplete request");
  }

  TEST(incomplete 2) {
    std::istringstream in("foo: bar");
    std::map<std::string, std::string> x;
    try {
      read_headers(in, x);
    } catch (remote_close&) {
      Equals(x.size(), 1U);
      Equals(x["foo"], "bar");
      return;
    }
    Check(!"incomplete request");
  }

  TEST(incomplete 3) {
    std::istringstream in("foo: bar\r\nxy");
    std::map<std::string, std::string> x;
    try {
      read_headers(in, x);
    } catch (remote_close&) {
      Equals(x.size(), 1U);
      Equals(x["foo"], "bar");
      return;
    }
    Check(!"incomplete request");
  }

  TEST(simple 1) {
    std::istringstream in("foo: bar\r\n\r\n");
    std::map<std::string, std::string> x;
    read_headers(in, x);
    Equals(x.size(), 1U);
    Equals(x["foo"], "bar");
  }

  TEST(simple 2) {
    std::istringstream in("a: 1\r\nB: 2\r\n\r\n");
    std::map<std::string, std::string> x;
    read_headers(in, x);
    Equals(x.size(), 2U);
    Equals(x["a"], "1");
    Equals(x["b"], "2");
  }

  TEST(multiline) {
    std::istringstream in("x: a\r\n b\r\n c\r\n     d\r\n\r\n");
    std::map<std::string, std::string> x;
    read_headers(in, x);
    Equals(x.size(), 1U);
    Equals(x["x"], "a b c d");
  }

  TEST(multiline incomplete) {
    std::istringstream in("x: a\r\n    ");
    std::map<std::string, std::string> x;
    try {
      read_headers(in, x);
    } catch (remote_close&) {
      Equals(x.size(), 1U);
      Equals(x["x"], "a ");
      return;
    }
    Check(!"incomplete request");
  }
}

TEST_GROUP(parse_http) {
  TEST_GROUP(parse_parametrised) {
    TEST(parse_parametrised) {
      std::string in =
        "application/x-test ; q = 0.7; boundary= \"\\\\\";z=\"\"";

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

    TEST(parse_parametrised #2) {
      std::string in = "_; a=\"1;2";

      std::string type;
      std::set<std::string> interests;
      interests.insert("a");

      std::map<std::string, std::string> parameters;
      parse_parametrised(in, type, interests, parameters);

      Equals(type, "_");

      Equals(parameters.size(), 1U);
      Equals(parameters["a"], "1;2");
    }

    TEST(parse_parametrised #3) {
      std::string in = "_; _=1,a=\"1;2";

      std::string type;
      std::set<std::string> interests;
      interests.insert("_");
      interests.insert("a");
      interests.insert("2");

      std::map<std::string, std::string> parameters;
      parse_parametrised(in, type, interests, parameters);

      Equals(type, "_");

      Equals(parameters.size(), 2U);
      Equals(parameters["_"], "1,a=\"1");
      Check(parameters.find("2") != parameters.end());
      Check(parameters["2"].empty());
    }
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

  XTEST(
      (name, "qvalue")
      (2tuples, 
        (std::string, int)
        ("", 1000)
        ("0.71", 710)
        (".71", 710)
        ("1.1", 1000)
        ("0", 0)
        ("1", 1000)
        (".999", 999)
        (".555555", 555)
        ("0.001", 1)
        (".001", 1)
        ("1.000", 1000)
        ("a.1 x2", 120)
        ("-1", 1000)
      ))
  {
    Equals(parse_qvalue(value.get<0>()), value.get<1>());
  }
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
