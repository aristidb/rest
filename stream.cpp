// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <istream>

rest::input_stream::~input_stream() {
  if (own)
    delete stream;
}
