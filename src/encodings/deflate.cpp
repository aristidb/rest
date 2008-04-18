// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/deflate.hpp>
#include <boost/iostreams/filter/zlib.hpp>

using rest::encodings::deflate;
namespace io = boost::iostreams;

std::string const &deflate::name() const {
  static std::string x("deflate");
  return x;
}

rest::object::name_list_type const &deflate::name_aliases() const {
  static name_list_type x;
  return x;
}

int deflate::priority() const {
  return -100;
}

void deflate::add_reader(input_chain &x) {
  io::zlib_params z;
  z.noheader = true;
  x.push(io::zlib_decompressor(z));
}

void deflate::add_writer(output_chain &x) {
  io::zlib_params z;
  z.noheader = true;
  x.push(io::zlib_compressor(z));
}
