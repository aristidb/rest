// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encodings/bzip2.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

using rest::encodings::bzip2;
namespace io = boost::iostreams;

std::string bzip2::name() const {
  return "bzip2";
}

std::vector<std::string> bzip2::aliases() const {
  return std::vector<std::string>(1, "x-bzip2");
}

void bzip2::add_reader(input_chain &x) {
  x.push(io::bzip2_decompressor());
}

void bzip2::add_writer(output_chain &x) {
  x.push(io::bzip2_compressor());
}
