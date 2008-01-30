// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_ENCODING_HPP
#define REST_ENCODING_HPP

#include <boost/iostreams/chain.hpp>
#include <boost/noncopyable.hpp>
#include <string>
#include <vector>
#include <memory>

namespace rest {

class encoding {
public:
  virtual std::string name() const = 0;

  virtual std::vector<std::string> aliases() const {
    return std::vector<std::string>();
  }

  typedef boost::iostreams::chain<boost::iostreams::input> input_chain;
  typedef boost::iostreams::chain<boost::iostreams::output> output_chain;

  virtual void add_reader(input_chain &) = 0;
  virtual void add_writer(output_chain &) = 0;

  virtual ~encoding() = 0;
};

class encodings_registry : boost::noncopyable {
public:
  static encodings_registry &get();

  void add_encoding(std::auto_ptr<encoding> &);
  encoding *find_encoding(std::string const &) const;

private:
  encodings_registry();
};

#define REST_ENCODING_ADD(e) \
  ::rest::encodings_registry::get().add_encoding( \
    ::std::auto_ptr<encoding>(new e))

}

#endif
