// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encoding.hpp>
#include <testsoon.hpp>

using rest::encoding;
using rest::encodings_registry;

TEST_GROUP(encodings_registry) {

TEST(invalid) {
  Equals(
    encodings_registry::get().find_encoding("NO-ENCODING"),
    (encoding*) 0);
}

TEST(identity) {
  encoding *enc = encodings_registry::get().find_encoding("identity");
  encoding *enc2 = encodings_registry::get().find_encoding("");
  Check(enc);
  Check(enc2);
  Equals(enc, enc2);
  Equals(enc->name(), "identity");
  Check(enc->is_identity());
}

}
