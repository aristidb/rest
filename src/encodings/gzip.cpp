// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/gzip.hpp>
#include <boost/iostreams/filter/gzip.hpp>

using rest::encodings::gzip;
namespace io = boost::iostreams;

std::string gzip::name() const {
  return "gzip";
}

std::vector<std::string> gzip::aliases() const {
  return std::vector<std::string>(1, "x-gzip");
}

void gzip::add_reader(input_chain &x) {
  x.push(io::gzip_decompressor());
}

void gzip::add_writer(output_chain &x) {
  x.push(io::gzip_compressor());
}
