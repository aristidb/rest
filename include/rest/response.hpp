// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_RESPONSE_HPP
#define REST_RESPONSE_HPP

#include <string>
#include <vector>
#include <iosfwd>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/cstdint.hpp>

namespace rest {

class cookie;
class input_stream;

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

  void set_code(int code);
  void set_type(std::string const &type);
  void set_header(std::string const &name, std::string const &value);
  void add_header_part(std::string const &name, std::string const &value);
  void list_headers(boost::function<void(std::string const &)> const &cb) const;

  void add_cookie(cookie const &c);

  enum content_encoding_t {
    identity,
    deflate,
    gzip,
    bzip2,
    X_NO_OF_ENCODINGS
  };

  void set_data(std::string const &data,
    content_encoding_t content_encoding = identity);
  void set_data(input_stream &data, bool seekable,
    content_encoding_t content_encoding = identity);

  void set_length(boost::int64_t size,
    content_encoding_t content_encoding = identity);

  int get_code() const;
  static char const *reason(int code);
  std::string const &get_type() const;

  bool has_content_encoding(content_encoding_t content_encoding) const;

  content_encoding_t choose_content_encoding(
    std::vector<content_encoding_t> const &encodings,
    bool ranges) const;

  bool is_nil(content_encoding_t content_encoding = identity) const;
  bool empty(content_encoding_t content_encoding = identity) const;
  bool chunked(content_encoding_t content_encoding) const;
  boost::int64_t length(content_encoding_t content_encoding) const;

  typedef std::vector<std::pair<boost::int64_t, boost::int64_t> > ranges_t;

  bool check_ranges(ranges_t const &ranges);

  void print_headers(std::ostream &out) const;
  void print_entity(
    std::streambuf &out,
    content_encoding_t enc,
    bool may_chunk,
    ranges_t const &ranges = ranges_t()) const;

private:
  void defaults();

  void print_cookie_header(std::ostream &out) const;
  void encode(
    std::streambuf &out, content_encoding_t enc, bool may_chunk,
    ranges_t const &ranges = ranges_t()) const;
  void decode(
    std::streambuf &out, content_encoding_t enc, bool may_chunk) const;

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
