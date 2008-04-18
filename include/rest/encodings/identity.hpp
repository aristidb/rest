// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_ENCODINGS_IDENTITY_HPP
#define REST_ENCODINGS_IDENTITY_HPP

#include <rest/encoding.hpp>

namespace rest { namespace encodings {

class identity : public encoding {
public:
  std::string const &name() const;
  name_list_type const &name_aliases() const;

  bool is_identity() const;

  int priority() const;

  void add_reader(input_chain &);
  void add_writer(output_chain &);
};

}}

#endif
