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
      // 500 - 5005
      "Internal Server Error", "Not Implemented", "Bad Gateway",
      "Service Unavailable", "Gateway Timeout", "HTTP Version Not Supported"
    };
  }

  char const *response::default_reason(int code) {
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
