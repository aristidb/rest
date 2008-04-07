// wegen ODR tests

#include <testsoon.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <cstring>

using namespace boost::lambda;

TEST() {
}

TEST_GROUP(yessir) {

TEST() {
} 

TEST_GROUP(nested) {

TEST() {
#ifdef __EXCEPTIONS
  throw 0;
#endif
  Check1(_1 > 4, 5);
}

XTEST() {
  Check1(_1 > 4, 0);
}

XTEST((name, "dummy") (name, "foo")) {
  //(void)fixture; - does not compile
}

struct dummy_fixture {
};

XTEST((fixture, dummy_fixture)) {
  Equals(typeid(fixture), typeid(dummy_fixture));
}

FTEST(, dummy_fixture) {
  Equals(typeid(fixture), typeid(dummy_fixture));
}

struct group_fixture_t {};

using testsoon::range_generator;

TEST_GROUP(_2_4) {

XTEST((gen, (range_generator<int>)(2)(4))) {
  Equals(value, 3);
}

}

TEST_GROUP(_4_6) {

XTEST((gen, (range_generator<int>)(4)(6))) {
  Equals(value, 4);
}

}

XTEST((gf, 1)) {
  Equals(typeid(group_fixture), typeid(group_fixture_t));
}

}}

XTEST((range, (int, 0, 4))) {
  Equals(value, 0);
}

char const * const array[] = { "abc", "xyz" };

XTEST((gen, (testsoon::array_generator<char const *>)(array))) {
  Equals(value, "");
}

XTEST((array, (char const *, array))) {
  Not_equals(value, "");
}

XTEST(
    (name, "VALUES!!!")
    (values, (int)(4)(7)(2))
) {
  Check(value && 0);
}

TEST_GROUP(tuples) {

XTEST((2tuples, (int, int)(4, 4)(5, 5))) {
  Equals(value.get<0>(), value.get<1>());
}

XTEST((2tuples, (char const *, std::size_t)("hey", 3)("hallo", 5))) {
  Equals(std::strlen(value.get<0>()), value.get<1>());
}

}
