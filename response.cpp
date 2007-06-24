// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include <boost/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
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

  struct data_holder {
    enum { NIL, STRING, STREAM } type;
    std::istream *stream;
    bool seekable;
    bool own_stream;
    std::string string;
    content_encoding_t compute_from;

    void set(std::string const &str) {
      type = STRING;
      string = str;
    }

    void set(std::istream *in, bool seekable_, bool own) {
      type = STREAM;
      stream = in;
      seekable = seekable_;
      own_stream = own;
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
      stream(0), seekable(false), own_stream(false),
      compute_from(identity) { }

    ~data_holder() {
      if (own_stream)
        delete stream;
    }
  };

  boost::array<data_holder, response::X_NO_OF_ENCODINGS> data;

  impl() : code(-1) { }
  impl(int code) : code(code) { }
  impl(std::string const &type) : code(-1), type(type) { }
};

response::response() : p(new impl) { }

response::response(int code) : p(new impl(code)) { }

response::response(std::string const &type) : p(new impl(type)) { }

response::response(std::string const &type, std::string const &data)
  : p(new impl(type))
{
  p->data[identity].set(data);
}

response::response(response const &r)
  : p(new impl(*r.p))
{ }

response::~response() { }

response &response::operator=(response const &lhs) {
  if(&lhs != this)
    p.reset(new impl(*lhs.p));
  return *this;
}

void response::set_code(int code) {
  p->code = code;
}

void response::set_type(std::string const &type) {
  p->type = type;
}

void response::set_header(std::string const &name, std::string const &value) {
  p->header[name] = value;
}

void response::add_header_part(
    std::string const &name, std::string const &value)
{
  impl::header_map::iterator i = p->header.find(name);
  if(i == p->header.end())
    set_header(name, value);
  else {
    i->second += ", ";
    i->second += value;
  }
}

void response::set_data(
    std::istream &data, bool seekable, content_encoding_t content_encoding)
{
  p->data[content_encoding].set(&data, seekable, false);
  if (p->data[identity].type == impl::data_holder::NIL)
    p->data[identity].compute_from = content_encoding;
}

void response::set_data(
    std::istream *data, bool seekable, content_encoding_t content_encoding)
{
  p->data[content_encoding].set(data, seekable, true);
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
    bool may_chunk
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
  if (!may_chunk)
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

void response::print(
    std::ostream &out, content_encoding_t enc, bool may_chunk) const
{
  impl::data_holder &d = p->data[enc];
  switch (d.type) {
  case impl::data_holder::STRING:
    out << d.string;
    break;
  case impl::data_holder::STREAM:
    {
      std::streambuf *in = d.stream->rdbuf();
      if (d.seekable || !may_chunk)
        std::copy(
          std::istreambuf_iterator<char>(in),
          std::istreambuf_iterator<char>(),
          std::ostreambuf_iterator<char>(out.rdbuf())
        );
      else {
        namespace io = boost::iostreams;
        io::filtering_ostreambuf out2;
        out2.push(utils::chunked_filter());
        out2.push(boost::ref(out));
        std::copy(
          std::istreambuf_iterator<char>(in),
          std::istreambuf_iterator<char>(),
          std::ostreambuf_iterator<char>(&out2)
        );
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
  print(out2, identity, false);
}

void response::decode(
    std::ostream &out, content_encoding_t enc, bool may_chunk) const
{
  //TODO: DECODING NOT IMPLEMENTED YET
}
