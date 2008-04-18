// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/gzip.hpp>
#include <boost/iostreams/filter/gzip.hpp>

using rest::encodings::gzip;
namespace io = boost::iostreams;

std::string const &gzip::name() const {
  static std::string x("gzip");
  return x;
}

rest::object::name_list_type const &gzip::name_aliases() const {
  static name_list_type x(1, "x-gzip");
  return x;
}

void gzip::add_reader(input_chain &x) {
  x.push(io::gzip_decompressor());
}

void gzip::add_writer(output_chain &x) {
  x.push(io::gzip_compressor());
}
