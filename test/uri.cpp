// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/utils/uri.hpp"
#include <boost/tuple/tuple_io.hpp>
#include <testsoon.hpp>

using namespace rest::utils::uri;

TEST_GROUP(escape) {
XTEST((values, (std::string)
       ("hallo")("?hallo+")("123&54$")
       (";/?:@&=+$,")("-_.!~*'()")))
{
  Equals(value, escape(value, false));
}

XTEST((values, (std::string)("ä")("é"))) {
  Not_equals(value, escape(value, false));
}

XTEST((2tuples, (std::string, std::string)("hallo welt", "hallo%20welt"))) {
  Equals(value.get<1>(), escape(value.get<0>(), false));
}

XTEST((values, (std::string)
       ("hallo")("~hallo!")("123_54.")
       ("-_.!~*'()")))
{
  Equals(value, escape(value, true));
}

XTEST((values, (std::string)(";/?:@&=+$,")("ä")("é"))) {
  Not_equals(value, escape(value, true));
}

XTEST((2tuples, (std::string, std::string)("hallo welt", "hallo%20welt"))) {
  Equals(value.get<1>(), escape(value.get<0>(), true));
}
}

TEST_GROUP(unescape) {
TEST(null) {
  Check(unescape("%00", false).empty());
}

TEST(form) {
  Equals(unescape("+", true), " ");
  Equals(unescape("+", false), "+");
}

XTEST((2tuples, (std::string, std::string)
       ("hallo%20welt", "hallo welt")))
{
  Equals(value.get<1>(), unescape(value.get<0>(), false));
}
}
