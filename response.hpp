#ifndef REST_RESPONSE_HPP
#define REST_RESPONSE_HPP

namespace rest {
  class response {
    int code;
  public:
    response(int code) : code(code) { }
    response(std::string const &type) {}
    void set_data(std::string const &data) {}
    void set_cookie(std::string const &name, std::string const &value) {}
    
    int getcode() const { return code; } //TEST only
  };
}

#endif
