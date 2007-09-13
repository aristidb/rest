// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include <boost/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/invert.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/null.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <map>
#include <cassert>

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
  typedef std::map<std::string, std::string> header_map;
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

    void set(std::string const &str) {
      type = STRING;
      string = str;
    }

    void set(input_stream &in, bool seekable_) {
      type = STREAM;
      in.move(stream);
      seekable = seekable_;
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
      switch (type) {
      case NIL:
        return true;
      case STRING:
        return false;
      case STREAM:
        return !seekable;
      }
      return true;
    }

    std::size_t length() const {
      switch (type) {
      case NIL:
        break;
      case STRING:
        return string.length();
      case STREAM:
        if (seekable) {
          std::size_t old_pos = stream->tellg();
          stream->seekg(0, std::ios::end);
          std::size_t new_pos = stream->tellg();
          stream->seekg(old_pos, std::ios::beg);
          return new_pos - old_pos;
        }
        break;
      }
      return 0;
    }

    data_holder() 
    : type(NIL),
      stream(0), seekable(false), compute_from(identity) { }
  };

  boost::array<data_holder, response::X_NO_OF_ENCODINGS> data;

  impl() : code(-1) { }
  impl(int code) : code(code) { }
  impl(std::string const &type) : code(-1), type(type) { }
};

response::response(empty_tag_t) {}

response::response() : p(new impl) { }

response::response(int code) : p(new impl(code)) { }

response::response(std::string const &type) : p(new impl(type)) { }

response::response(std::string const &type, std::string const &data)
  : p(new impl(type))
{
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

void response::set_code(int code) {
  p->code = code;
}

void response::set_type(std::string const &type) {
  p->type = type;
}

void response::set_header(std::string const &name_, std::string const &value) {
  std::string name = name_;
  boost::algorithm::to_lower(name);
  p->header[name] = value;
}

void response::add_header_part(
    std::string const &name_, std::string const &value)
{
  std::string name = name_;
  boost::algorithm::to_lower(name);
  impl::header_map::iterator i = p->header.find(name);
  if(i == p->header.end())
    set_header(name, value);
  else {
    i->second += ", ";
    i->second += value;
  }
}

void response::add_cookie(cookie const &c) {
  p->cookies.erase(c.name);
  p->cookies.insert(c);
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
    std::vector<content_encoding_t> const &encodings
  ) const
{
  if (encodings.empty() || empty(identity))
    return identity;
  typedef std::vector<content_encoding_t>::const_iterator iterator;
  for (iterator it = encodings.begin(); it != encodings.end(); ++it)
    if (has_content_encoding(*it))
      return *it;
  if (p->data[identity].type == impl::data_holder::NIL)
    return identity;
  std::size_t length = p->data[identity].length();
  if (length != 0 && length <= 10000)
    return identity;
  return encodings[0];
}

bool response::empty(content_encoding_t enc) const {
  if (p->data[enc].type == impl::data_holder::NIL)
    return p->data[enc].compute_from == enc;
  return p->data[enc].empty();
}

bool response::chunked(content_encoding_t enc) const {
  return !empty(enc) && p->data[enc].chunked();
}

std::size_t response::length(content_encoding_t enc) const {
  return p->data[enc].length();
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
  out << "\r\n";
}

void response::print_headers(std::ostream &out) const {
  /* TODO:
     Should we set a cache control header per default?
   */
  for (impl::header_map::const_iterator it = p->header.begin();
      it != p->header.end();
      ++it)
    out << it->first << ": " << it->second << "\r\n";
  print_cookie_header(out);
  out << "\r\n";
}

void response::print_entity(
    std::ostream &out, content_encoding_t enc, bool may_chunk) const
{
  impl::data_holder &d = p->data[enc];
  switch (d.type) {
  case impl::data_holder::STRING:
    out << d.string;
    break;
  case impl::data_holder::STREAM:
    {
      namespace io = boost::iostreams;
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
    std::ostream &out, content_encoding_t enc, bool may_chunk) const
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
  print_entity(out2, identity, false);
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
    in.push(io::array_source(d.string.data(), d.string.length()));
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
