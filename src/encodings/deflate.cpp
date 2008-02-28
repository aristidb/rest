// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/deflate.hpp>
#include <boost/iostreams/filter/zlib.hpp>

using rest::encodings::deflate;
namespace io = boost::iostreams;

std::string deflate::name() const {
  return "deflate";
}

std::vector<std::string> deflate::aliases() const {
  return std::vector<std::string>();
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
