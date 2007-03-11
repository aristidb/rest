// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <testsoon.hpp>
#include <iostream>
#include "rest.hpp"

TEST_REGISTRY

int main() {
  testsoon::default_reporter rep(std::cout);
  testsoon::tests().run(rep);
}
