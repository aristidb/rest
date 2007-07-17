// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <istream>

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
