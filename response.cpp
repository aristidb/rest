// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/input_stream.hpp"
#include "rest/output_stream.hpp"
#include "rest/rest.hpp"
#include "rest/utils.hpp"
#include "rest/config.hpp"
#include <boost/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/invert.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/null.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <map>
#include <algorithm>
#include <cassert>
#include<iostream>//FIXME

using rest::response;

namespace {
  char const *default_reasons[] = {
    // 100 - 101
    "Continue", "Switching Protocols",
    // 200 - 206
    "OK", "Created", "Accepted", "Non-Authoritative Information",
    "No Content", "Reset Content", "Partial Content",
    // 300 - 307
    "Multiple Choices", "Moved Permanently", "Found", "See Other",
    "Not Modified", "Use Proxy", "(Unused)", "Temporary Redirect",
    // 400 - 417
    "Bad Request", "Unauthorized", "Payment Required", "Forbidden",
    "Not Found", "Method Not Allowed", "Not Acceptable",
    "Proxy Authentication Required", "Request Timeout", "Conflict",
    "Gone", "Length Required", "Precondition Failed",
    "Request Entity Too Large", "Request-URI Too Long",
    "Unsupported Media Type", "Requested Range Not Satisfiable",
    "Expectation Failed",
    // 500 - 505
    "Internal Server Error", "Not Implemented", "Bad Gateway",
    "Service Unavailable", "Gateway Timeout", "HTTP Version Not Supported"
  };
}

struct response::impl {
  int code;
  std::string type;
  std::string boundary;

  typedef std::map<std::string, std::string,
                   rest::utils::string_icompare>
          header_map;

  header_map header;

  typedef boost::multi_index_container<
      cookie,
      boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
          boost::multi_index::member<cookie, std::string, &cookie::name>
        >
      >
    > cookie_set;
  cookie_set cookies;

  struct data_holder {
    enum { NIL, STRING, STREAM } type;
    input_stream stream;
    bool seekable;
    std::string string;
    content_encoding_t compute_from;
    boost::int64_t length;

    void set(std::string const &str) {
      type = STRING;
      string = str;
      length = string.size();
    }

    void set(input_stream &in, bool seekable_) {
      type = STREAM;
      in.move(stream);
      seekable = seekable_;
      if (seekable) {
        boost::uint64_t old_pos = stream->tellg();
        stream->seekg(0, std::ios::end);
        boost::uint64_t new_pos = stream->tellg();
        stream->seekg(old_pos, std::ios::beg);
        length = new_pos - old_pos;
      } else {
        length = -1;
      }
    }

    bool empty() const {
      switch (type) {
      case NIL:
        return true;
      case STRING:
        return string.empty();
      case STREAM:
        stream->peek();
        return !*stream;
      }
      return true;
    }

    bool chunked() const {
      return length < 0;
    }

    data_holder() 
    : type(NIL),
      stream(0),
      seekable(false),
      compute_from(identity),
      length(-1) { }
  };

  boost::array<data_holder, response::X_NO_OF_ENCODINGS> data;

  impl() : code(-1) { }
  impl(int code) : code(code) { }
  impl(std::string const &type) : code(-1), type(type) { }
};

response::response(empty_tag_t) {}

response::response() : p(new impl) {
  defaults();
}

response::response(int code) : p(new impl(code)) {
  defaults();
}

response::response(std::string const &type) : p(new impl(type)) {
  defaults();
}

response::response(std::string const &type, std::string const &data)
  : p(new impl(type))
{
  defaults();
  p->data[identity].set(data);
}

response::response(response &o) {
  p.swap(o.p);
}

response::~response() { }

void response::move(response &o) {
  if (this == &o)
    return;
  p.swap(o.p);
  p.reset();
}

void response::swap(response &o) {
  p.swap(o.p);
}

void response::defaults() {
  set_header("Content-Type", p->type);

  // Make sure that these headers exist before they are examined for caching:
  set_header("Expires", "");
  set_header("Cache-Control", "");
  set_header("Pragma", "");
}

void response::set_code(int code) {
  p->code = code;
}

void response::set_type(std::string const &type) {
  p->type = type;
  set_header("Content-Type", type);
}

void response::set_header(std::string const &name, std::string const &value) {
  p->header[name] = value;
}

void response::add_header_part(
    std::string const &name, std::string const &value)
{
  impl::header_map::iterator i = p->header.find(name);
  if(i == p->header.end() || i->second.empty()) {
    set_header(name, value);
  } else {
    i->second += ", ";
    i->second += value;
  }
}

void response::list_headers(
    boost::function<void (std::string const &)> const &cb) const
{
  for (impl::header_map::iterator it = p->header.begin();
      it != p->header.end();
      ++it)
    cb(it->first);
}

void response::add_cookie(cookie const &c) {
  p->cookies.erase(c.name);
  p->cookies.insert(c);

  // Set-Cookie headers are not set like normal headers, fake the existence
  // of a Set-Cookie header:
  set_header("Set-Cookie", "");
  //set_header("Set-Cookie2", ""); -- unused
}

void response::set_data(
    input_stream &data, bool seekable, content_encoding_t content_encoding)
{
  p->data[content_encoding].set(data, seekable);
  if (p->data[identity].type == impl::data_holder::NIL)
    p->data[identity].compute_from = content_encoding;
}

void response::set_data(
    std::string const &data, content_encoding_t content_encoding)
{
  p->data[content_encoding].set(data);
  if (p->data[identity].type == impl::data_holder::NIL)
    p->data[identity].compute_from = content_encoding;
}

int response::get_code() const {
  return p->code;
}

char const *response::reason(int code) {
  if(code == 100 || code == 101)
    return default_reasons[code - 100];
  else if(code >= 200 && code <= 206)
    return default_reasons[code - 198];
  else if(code >= 300 && code <= 307)
    return default_reasons[code - 291];
  else if(code >= 400 && code <= 417)
    return default_reasons[code - 383];
  else if(code >= 500 && code <= 505)
    return default_reasons[code - 465];
  else
    return "";
}

std::string const &response::get_type() const {
  return p->type;
}

bool response::has_content_encoding(content_encoding_t content_encoding) const {
  return p->data[content_encoding].type != impl::data_holder::NIL;
}

response::content_encoding_t
response::choose_content_encoding(
    std::vector<content_encoding_t> const &encodings,
    bool ranges
  ) const
{
  if (encodings.empty() || empty(identity))
    return identity;
  typedef std::vector<content_encoding_t>::const_iterator iterator;
  if (!ranges) {
    for (iterator it = encodings.begin(); it != encodings.end(); ++it)
      if (has_content_encoding(*it))
        return *it;
  }
  if (p->data[identity].type == impl::data_holder::NIL)
    return identity;
  boost::int64_t length = p->data[identity].length;

  if (length >= 0) {
    rest::utils::property_tree &conf = rest::config::get().tree();

    boost::int64_t min_length =
      rest::utils::get(conf, boost::int64_t(0),
        "general", "compression", "minimum_size");
    if (min_length < 0)
      min_length = 0;

    if (length <= min_length)
      return identity;
  }

  return encodings[0];
}

bool response::is_nil(content_encoding_t enc) const {
  return p->data[enc].type == impl::data_holder::NIL &&
         p->data[enc].compute_from == enc;
}

bool response::empty(content_encoding_t enc) const {
  if (p->data[enc].type == impl::data_holder::NIL)
    return p->data[enc].compute_from == enc;
  return p->data[enc].empty();
}

bool response::chunked(content_encoding_t enc) const {
  return !empty(enc) && p->data[enc].chunked();
}

boost::int64_t response::length(content_encoding_t enc) const {
  return empty(enc) ? 0 : p->data[enc].length;
}

void response::set_length(boost::int64_t len, content_encoding_t enc) {
  p->data[enc].length = len;
}

bool response::check_ranges(ranges_t const &ranges) {
  if (ranges.empty())
    return true;
  boost::int64_t length = this->length(identity);
  if (length < 0)
    return false;
  for (ranges_t::const_iterator it = ranges.begin(); it != ranges.end(); ++it) {
    if (it->first >= length)
      return false;
    if (it->second >= length)
      return false;
  }
  set_code(206);
  if (ranges.size() > 1) {
    p->boundary = utils::http::random_boundary();
    set_header("Content-Type", "multipart/byte-ranges;boundary=" + p->boundary);
  } else {
    std::pair<boost::int64_t, boost::int64_t> x = ranges[0];
    if (x.first < 0)
      x.first = 0;
    if (x.second < 0)
      x.second = length;
    std::ostringstream range;
    range << "bytes " << x.first << '-' << x.second << '/' << length;
    set_header("Content-Range", range.str());
  }
  return true;
}

namespace {
#if 0
  void print_cookie2(std::ostream &out, rest::cookie const &c, bool first) {
    // Set-Cookie2 [see RFC 2965]
    if(first)
      out << "Set-Cookie2: ";
    else
      out << ',';
    out << c.name << "=\"" << c.value << "\";Version=\"1\"";
    if(!c.comment.empty())
      out << ";Comment=\"" << c.comment << '"';
    if(!c.comment_url.empty())
      out << ";CommentURL=\"" << c.comment_url << '"';
    if(c.discard)
      out << ";Discard";
    if(!c.domain.empty())
      out << ";Domain=\"" << c.domain << '"';
    if(c.max_age >= 0)
      out << ";Max-Age=\"" << c.max_age << '"';
    if(!c.path.empty())
      out << ";Path=\"" << c.path << '"';
    if(!c.ports.empty()) {
      out << ";Ports=\"";
      typedef rest::cookie::port_list::const_iterator port_iterator;
      port_iterator const end = c.ports.end();
      port_iterator const begin = c.ports.begin();
      for(port_iterator j = begin; j != end; ++j) {
        if(j != begin)
          out << ',';
        out << *j;
      }
      out << '"';
    }
    if(c.secure)
      out << ";Secure";
  }
#endif

  void print_cookie(std::ostream &out, rest::cookie const &c) {
    // Set-Cookie [see Netscape Spec
    //             http://wp.netscape.com/newsref/std/cookie_spec.html ]

    out << "Set-Cookie: ";
    out << rest::utils::uri::escape(c.name) << '=';
    out << rest::utils::uri::escape(c.value);
    if(c.expires != time_t(-1))
      out << ";expires="
          << rest::utils::http::datetime_string(c.expires);
    if(!c.domain.empty())
      out << ";domain=" << c.domain;
    if(!c.path.empty())
      out << ";path=" << c.path;
    if(c.secure)
      out << ";secure";
    out << "\r\n";
  }
}

void response::print_cookie_header(std::ostream &out) const {
  typedef impl::cookie_set::const_iterator cookie_iterator;
  cookie_iterator const begin = p->cookies.begin();
  cookie_iterator const end = p->cookies.end();
  for(cookie_iterator i = begin; i != end; ++i) {
    print_cookie(out, *i);
  }
}

void response::print_headers(std::ostream &out) const {
  p->header.erase("set-cookie");
  for (impl::header_map::const_iterator it = p->header.begin();
      it != p->header.end();
      ++it)
    if (!it->second.empty())
      out << it->first << ": " << it->second << "\r\n";
  print_cookie_header(out);
  out << "\r\n";
}

void response::print_entity(
    std::ostream &out,
    content_encoding_t enc,
    bool may_chunk,
    ranges_t const &ranges) const
{
  namespace io = boost::iostreams;

  impl::data_holder &d = p->data[enc];

  if (!ranges.empty()) {
    io::filtering_ostream out2;
    if (may_chunk)
      out2.push(utils::chunked_filter());
    out2.push(boost::ref(out));

    if (enc != identity) {
      encode(out2, enc, false, ranges);
      return;
    }

    boost::int64_t length = this->length(identity);
    assert(length >= 0);

    if (ranges.size() == 1) {
      std::pair<boost::int64_t, boost::int64_t> x = ranges[0];
      if (x.first < 0)
        x.first = 0;
      if (x.second < 0)
        x.second = length;

      switch (d.type) {
      case impl::data_holder::STRING:
        out2 << d.string.substr(x.first, x.second - x.first);
        break;
      case impl::data_holder::STREAM:
        io::copy(
          io::restrict(*d.stream->rdbuf(), x.first, x.second - x.first),
          out2);
        break;
      default:
        assert(false);
        break;
      }
      return;
    } else {
      for (ranges_t::const_iterator it = ranges.begin();
          it != ranges.end();
          ++it)
      {
        out2 << "\r\n--" << p->boundary << "\r\n";
        out2 << "Content-Type: " << p->type << "\r\n";

        std::pair<boost::int64_t, boost::int64_t> x = *it;
        if (x.first < 0)
          x.first = 0;
        if (x.second < 0)
          x.second = length;
        out2 << "Content-Range: ";
        out2 << "bytes " << x.first << '-' << x.second << '/' << length;
        out2 << "\r\n\r\n";

        print_entity(out2, identity, false, ranges_t(1, x));
      }
      out2 << "\r\n--" << p->boundary << "--\r\n";
      return;
    }
  }

  switch (d.type) {
  case impl::data_holder::STRING:
    out << d.string;
    break;
  case impl::data_holder::STREAM:
    {
      std::streambuf &in = *d.stream->rdbuf();
      if (d.seekable || !may_chunk)
        io::copy(in, out);
      else {
        io::filtering_ostreambuf out2;
        out2.push(utils::chunked_filter());
        out2.push(boost::ref(out));
        io::copy(in, out2);
      }
    }
    break;
  case impl::data_holder::NIL:
    if (d.compute_from == identity) {
      if (enc != identity)
        encode(out, enc, may_chunk);
    } else {
      assert(enc == identity);
      decode(out, d.compute_from, may_chunk);
    }
  }
}

void response::encode(
    std::ostream &out, content_encoding_t enc, bool may_chunk,
    ranges_t const &ranges) const
{
  namespace io = boost::iostreams;
  io::filtering_ostream out2;
  switch (enc) {
  case deflate:
    out2.push(io::zlib_compressor());
    break;
  case gzip:
    out2.push(io::gzip_compressor());
    break;
  case bzip2:
    out2.push(io::bzip2_compressor());
    break;
  default:
    assert(false);
    break;
  }
  if (may_chunk)
    out2.push(utils::chunked_filter());
  out2.push(boost::ref(out));
  print_entity(out2, identity, false, ranges);
}

void response::decode(
    std::ostream &out, content_encoding_t enc, bool may_chunk) const
{
  namespace io = boost::iostreams;
  io::filtering_istream in;
  switch (enc) {
  case deflate:
    in.push(io::zlib_decompressor());
    break;
  case gzip:
    in.push(io::gzip_decompressor());
    break;
  case bzip2:
    in.push(io::bzip2_decompressor());
    break;
  default:
    assert(false);
    break;
  }

  impl::data_holder &d = p->data[enc];
  switch (d.type) {
  case impl::data_holder::NIL:
    in.push(io::null_source());
    break;
  case impl::data_holder::STRING:
    in.push(io::array_source(d.string.data(), d.length));
    break;
  case impl::data_holder::STREAM:
    in.push(boost::ref(*d.stream));
    break;
  }

  io::filtering_ostream out2;
  if (may_chunk)
    out2.push(utils::chunked_filter());
  out2.push(boost::ref(out));

  io::copy(in, out2);
}
