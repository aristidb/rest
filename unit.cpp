// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <testsoon.hpp>
#include <iostream>

int main() {
#ifndef NDEBUG
  testsoon::default_reporter rep(std::cout);
  testsoon::tests().run(rep);
#else
  std::cerr << "Sorry, no tests in RELEASE mode.\n";
#endif
}
