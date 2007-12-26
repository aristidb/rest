// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/keywords.hpp>
#include <testsoon.hpp>

using rest::keywords;

TEST(case insensitivity) {
  keywords kw;
  kw.set("Xy", "ab");
  std::string x;
  Nothrows(x = kw["xY"], ...);
  Equals(x, "ab");
}
