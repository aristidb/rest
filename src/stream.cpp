// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/input_stream.hpp"
#include "rest/output_stream.hpp"
#include <istream>
#include <ostream>

void rest::input_stream::reset() {
  if (own)
    delete stream;
  stream = 0;
  own = false;
}

void rest::output_stream::reset() {
  if (own)
    delete stream;
  stream = 0;
  own = false;
}
