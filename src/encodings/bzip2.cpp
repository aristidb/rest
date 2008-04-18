// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/bzip2.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

using rest::encodings::bzip2;
namespace io = boost::iostreams;

std::string const &bzip2::name() const {
  static std::string x = "bzip2";
  return x;
}

rest::object::name_list_type const &bzip2::name_aliases() const {
  static name_list_type x(1, "x-bzip2");
  return x;
}

void bzip2::add_reader(input_chain &x) {
  x.push(io::bzip2_decompressor());
}

void bzip2::add_writer(output_chain &x) {
  x.push(io::bzip2_compressor());
}
