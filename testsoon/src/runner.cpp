#include <testsoon.hpp>
#include <testsoon/xml_reporter.hpp>
#include <iostream>

TEST_REGISTRY

int main() {
  //testsoon::default_reporter rep(std::cout);
  testsoon::xml_reporter rep(std::cout);

  return !testsoon::tests().run(rep);
}
