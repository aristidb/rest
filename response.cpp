#include "rest.hpp"

namespace rest {
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
#ifdef OLD_RESPONSE
  }
  char const *response::default_reason(int code) {
#else
    char const *default_reason(int code) {
#endif
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
#ifndef OLD_RESPONSE
  }

  struct response::impl {
    int code;
    std::string type;

    impl() { }
    impl(int code) : code(code) { }
    impl(std::string const &type) : type(type) { }
  };

  response::response() : p(new impl) { }
  response::response(int code) : p(new impl(code)) { }
  response::response(std::string const &type) : p(new impl(type)) { }
  response::response(std::string const &type, std::string const &data)
    : p(new impl(type)) { }
  response::response(response const &r)
    : p(new impl(*r.p)) { }
  response::~response() { }

  response &response::operator=(response const &lhs) {
    if(&lhs != this) {
      p.reset(new impl(*lhs.p));
    }
    return *this;
  }


  void response::set_code(int code) { p->code = code; }
  void response::set_type(std::string const &type) { p->type = type; }
  void response::set_header(std::string const &name, std::string const &value)
  { }
  void response::add_header_part(std::string const &name,
                                 std::string const &value)
  { }

  void response::set_data(std::istream &data, bool seekable, content_encoding_t content_encoding) { }
  void response::set_data(std::string const &data, content_encoding_t content_encoding) { }

  int response::get_code() const {
    return p->code;
  }

  char const *response::get_reason() const {
    return default_reason(p->code);
  }

  std::string const &response::get_type() const {
    return p->type;
  }

  bool response::has_content_encoding(content_encoding_t content_encoding) const { }

  std::string const &response::get_data() const { return ""; }
#endif
}
