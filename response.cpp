// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"

#include <boost/array.hpp>
#include <map>

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

  char const *default_reason(int code) {
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
    else // throw exception!
      return "";
  }
}

struct response::impl {
  int code;
  std::string type;
  typedef std::map<std::string, std::string> header_map;
  header_map header;

  struct data_holder {
    enum { NIL, STRING, STREAM } type;
    std::pair<std::istream*, bool> stream;
    std::string string;
    content_encoding_t compute_from;

    void set(std::string const &str) {
      type = STRING;
      string = str;
    }

    void set(std::istream &in, bool seekable) {
      type = STREAM;
      stream = std::make_pair(&in, seekable);
    }

    bool empty() const {
      switch (type) {
      case NIL:
        return true;
      case STRING:
        return string.empty();
      case STREAM:
        stream.first->peek();
        return !*stream.first;
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
        return !stream.second;
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
        if (stream.second) {
          std::istream &i = *stream.first;
          std::size_t old_pos = i.tellg();
          i.seekg(0, std::ios::end);
          std::size_t new_pos = i.tellg();
          i.seekg(old_pos, std::ios::beg);
          return new_pos - old_pos;
        }
        break;
      }
      return 0;
    }

    data_holder() : type(NIL), compute_from(identity) { }
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

char const *response::get_reason() const {
  return default_reason(p->code);
}

std::string const &response::get_type() const {
  return p->type;
}

bool response::has_content_encoding(content_encoding_t content_encoding) const {
  return p->data[content_encoding].type != impl::data_holder::NIL;
}

std::string const &response::get_data() const { return p->data[0].string; }

response::content_encoding_t
response::choose_content_encoding(
    std::vector<content_encoding_t> const &encodings) const
{
  if (encodings.empty() || empty(identity))
    return identity;
  typedef std::vector<content_encoding_t>::const_iterator iterator;
  for (iterator it = encodings.begin(); it != encodings.end(); ++it)
    if (has_content_encoding(*it))
      return *it;
  if (p->data[identity].type == impl::data_holder::NIL)
    return identity;
  else
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

void response::print(std::ostream &out, content_encoding_t enc) const {
}
