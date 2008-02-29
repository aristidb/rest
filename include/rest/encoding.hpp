// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_ENCODING_HPP
#define REST_ENCODING_HPP

#include <boost/iostreams/chain.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace rest {

class encoding {
public:
  virtual std::string name() const = 0;

  virtual std::vector<std::string> aliases() const {
    return std::vector<std::string>();
  }

  virtual bool is_identity() const { return false; }

  virtual int priority() const { return 0; }

  typedef boost::iostreams::chain<boost::iostreams::input> input_chain;
  typedef boost::iostreams::chain<boost::iostreams::output> output_chain;

  virtual void add_reader(input_chain &) = 0;
  virtual void add_writer(output_chain &) = 0;

  virtual ~encoding() = 0;
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

class encodings_registry : boost::noncopyable {
public:
  static encodings_registry &get();

  void add_encoding(std::auto_ptr<encoding>);
  encoding *find_encoding(std::string const &) const;

private:
  encodings_registry();

  class impl;
  boost::scoped_ptr<impl> p;
};

#define REST_ENCODING_ADD(e) \
  ::rest::encodings_registry::get().add_encoding( \
    ::std::auto_ptr< ::rest::encoding>(new e))

}

#endif
