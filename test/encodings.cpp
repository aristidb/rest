// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encoding.hpp>
#include <testsoon.hpp>

using rest::encoding;
using rest::object_registry;

TEST_GROUP(object_registry) {

TEST(invalid) {
  Equals(
    object_registry::get().find<encoding>("NO-ENCODING"),
    (encoding*) 0);
}

TEST(identity) {
  encoding *enc = object_registry::get().find<encoding>("identity");
  encoding *enc2 = object_registry::get().find<encoding>("");
  Check(enc);
  Check(enc2);
  Equals(enc, enc2);
  Equals(enc->name(), "identity");
  Check(enc->is_identity());
}

}
