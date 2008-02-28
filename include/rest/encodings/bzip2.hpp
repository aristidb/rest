// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_ENCODINGS_BZIP2_HPP
#define REST_ENCODINGS_BZIP2_HPP

#include <rest/encoding.hpp>

namespace rest { namespace encodings {

class bzip2 : public encoding {
public:
  std::string name() const;
  std::vector<std::string> aliases() const;

  void add_reader(input_chain &);
  void add_writer(output_chain &);
};

}}

#endif
