// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_ENCODING_HPP
#define REST_ENCODING_HPP

#include "object.hpp"
#include <boost/iostreams/chain.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace rest {

class encoding : public object {
public:
  static std::string const &type_name();

  static bool need_load_standard_objects;
  static void load_standard_objects(object_registry &obj_reg);

  std::string const &type() const { return type_name(); }

  virtual ~encoding() = 0;

  virtual bool is_identity() const { return false; }

  virtual int priority() const { return 0; }

  typedef boost::iostreams::chain<boost::iostreams::input> input_chain;
  typedef boost::iostreams::chain<boost::iostreams::output> output_chain;

  virtual void add_reader(input_chain &) = 0;
  virtual void add_writer(output_chain &) = 0;
};

struct compare_encoding {
  bool operator()(encoding *a, encoding *b) const {
    int ap = a->priority();
    int bp = b->priority();
    if (ap < bp)
      return true;
    else if (bp < ap)
      return false;
    return a < b;
  }
};

struct invalid_encoding : std::logic_error {
  invalid_encoding(std::string const &enc) 
    : std::logic_error("invalid encoding: " + enc)
  {}
};

}

#endif
