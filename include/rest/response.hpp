// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_RESPONSE_HPP
#define REST_RESPONSE_HPP

#include "encoding.hpp"
#include <string>
#include <vector>
#include <set>
#include <iosfwd>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/cstdint.hpp>

namespace rest {

class cookie;
class input_stream;
class headers;

/*
 * TODO:
 * - multi-chunk output
 * - mmap output / sendfile
 */
class response {
public:
  response();
  response(int code);
  response(std::string const &type);
  response(std::string const &type, std::string const &data);
  response(int code, std::string const &type, std::string const &data);
  response(response &r);
  ~response();

  operator response &() { return *this; }

  struct empty_tag_t {};
  response(empty_tag_t);
  static empty_tag_t empty_tag() { return empty_tag_t(); }

  void move(response &o);
  void swap(response &o);

  void set(int code, std::string const &type, std::string const &data) {
    set_code(code);
    set_type(type);
    set_data(data);
  }
  void set_code(int code);
  void set_type(std::string const &type);

  headers &get_headers();

  headers const &get_headers() const {
    return const_cast<response *>(this)->get_headers();
  }

  void add_cookie(cookie const &c);

  void set_data(std::string const &data, encoding *enc = 0);
  void set_data(std::string const &data, std::string const &enc);
  void set_data(input_stream &data, bool seekable, encoding *enc = 0);
  void set_data(input_stream &data, bool seekable, std::string const &enc);

  void set_length(boost::int64_t size, encoding *enc);
  void set_length(boost::int64_t size, std::string const &enc);

  int get_code() const;
  static char const *reason(int code);
  std::string const &get_type() const;

  bool has_content_encoding(encoding *enc) const;
  bool has_content_encoding(std::string const &enc) const;

  encoding *choose_content_encoding(
    std::set<encoding *, compare_encoding> const &encodings,
    bool ranges) const;

  bool is_nil(encoding *content_encoding = 0) const;
  bool empty(encoding *content_encoding = 0) const;
  bool chunked(encoding *content_encoding) const;
  boost::int64_t length(encoding *content_encoding = 0) const;

  typedef std::vector<std::pair<boost::int64_t, boost::int64_t> > ranges_t;

  bool check_ranges(ranges_t const &ranges);

  void print_headers(std::ostream &out);

  void print_entity(
    std::streambuf &out,
    encoding *enc,
    bool may_chunk,
    ranges_t const &ranges = ranges_t()) const;

private:
  void defaults();

  void print_cookie_header(std::ostream &out) const;
  void encode(
    std::streambuf &out, encoding *enc, bool may_chunk,
    ranges_t const &ranges = ranges_t()) const;
  void decode(
    std::streambuf &out, encoding *enc, bool may_chunk) const;

  class impl;
  boost::scoped_ptr<impl> p;

  response &operator=(response const &); //DUMMY
};

}

#endif
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
