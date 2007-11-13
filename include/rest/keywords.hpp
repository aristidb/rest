// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_KEYWORDS_HPP
#define REST_KEYWORDS_HPP

#include <string>
#include <boost/scoped_ptr.hpp>

namespace rest {

class input_stream;
class output_stream;
class request;

enum keyword_type {
  NORMAL,
  COOKIE,
  ENTITY,
  FORM_PARAMETER,
  HEADER,
  NONE = -1
};

class keywords {
public:
  keywords();
  ~keywords();

  bool exists(std::string const &key, int index = 0) const;

  std::string const &get(std::string const &keyword, int index = 0) const {
    return const_cast<keywords *>(this)->access(keyword, index);
  }

  std::string &access(std::string const &key, int index = 0);
  std::istream &read(std::string const &key, int index = 0);

  std::string get_name(std::string const &key, int index = 0) const;

  void declare(std::string const &key, int index, keyword_type type);
  keyword_type get_declared_type(std::string const &key, int index = 0) const;

  bool is_set(std::string const &key, int index = 0) const;

  void set(std::string const &key, int index, std::string const &value);
  void set_with_type(
    keyword_type, std::string const &key, int index, std::string const &value);
  void set_stream(std::string const &key, int index, input_stream &stream);
  void set_name(std::string const &key, int index, std::string const &name);
  void set_output(std::string const &key, int index, output_stream &output);

  void unset(std::string const &key, int index = 0);

  void flush();

  void set_entity(input_stream &entity, std::string const &type);
  void add_uri_encoded(std::string const &data);

  void set_request_data(request const &req);

public:
  std::string &operator[](std::string const &key) {
    return access(key);
  }

  void declare(std::string const &key, keyword_type type) {
    return declare(key, 0, type);
  }

  void set(std::string const &key, std::string const &value) {
    return set(key, 0, value);
  }

  void set_with_type(
      keyword_type type, std::string const &key, std::string const &value)
  {
    return set_with_type(type, key, 0, value);
  }

  void set_stream(std::string const &key, input_stream &value) {
    return set_stream(key, 0, value);
  }
  
  void set_name(std::string const &key, std::string const &value) {
    return set_name(key, 0, value);
  }

  void set_output(std::string const &key, output_stream &stream) {
    return set_output(key, 0, stream);
  }

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
