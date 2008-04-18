// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_ENCODINGS_GZIP_HPP
#define REST_ENCODINGS_GZIP_HPP

#include <rest/encoding.hpp>

namespace rest { namespace encodings {

class gzip : public encoding {
public:
  std::string const &name() const;
  name_list_type const &name_aliases() const;

  void add_reader(input_chain &);
  void add_writer(output_chain &);
};

}}

#endif
