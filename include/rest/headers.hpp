// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_HEADERS_HPP
#define REST_HEADERS_HPP

#include <iosfwd>
#include <string>
#include <sstream>
#include <boost/optional.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest {

class headers {
public:
  headers();
  headers(std::streambuf &in);
  headers(headers const &);
  ~headers();

  headers &operator=(headers const &o) {
    headers(o).swap(*this);
    return *this;
  }

  void swap(headers &o) {
    p.swap(o.p);
  }

public:
  void set_header(std::string const &name, std::string const &value);

  template<class T>
  void set_header(std::string const &name, T const &value) {
    std::ostringstream stream;
    stream << value;
    this->set_header(name, stream.str());
  }

  void add_header_part(
    std::string const &name,
    std::string const &value,
    bool special_asterisk = true);

  boost::optional<std::string> get_header(std::string const &name) const;
  std::string get_header(
      std::string const &name, std::string const &default_) const;

  void erase_header(std::string const &name);

  typedef
    boost::function<void (std::string const &name, std::string const &value)>
    header_callback;

  void for_each_header(header_callback const &) const;

  void read_headers(std::streambuf &in);
  void write_headers(std::streambuf &out) const;

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
