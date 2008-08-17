// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/keywords.hpp"
#include "rest/headers.hpp"
#include "rest/request.hpp"
#include "rest/cookie.hpp"
#include "rest/input_stream.hpp"
#include "rest/output_stream.hpp"
#include "rest/utils/http.hpp"
#include "rest/utils/boundary_filter.hpp"
#include "rest/utils/uri.hpp"
#include "rest/utils/string.hpp"
#include <boost/unordered_map.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>
#include <stdexcept>
#include <sstream>
#include <map>
#include <set>
#include <limits>
#include <cstdio>
#include <cctype>
#include <memory>

using namespace rest;
namespace uri = rest::utils::uri;
namespace io = boost::iostreams;

namespace {
  struct keyword_data {
    enum state_t { s_normal, s_prepared, s_unset = -1 };

    keyword_data(keyword_type type = NORMAL)
    : type(type), state(s_unset)
    {}

    keyword_data(keyword_data const &o)
    : type(o.type), state(s_unset)
    {}

    void read() {
      if (stream.get()) {
        if (output.get()) {
          *output << stream->rdbuf();
          output.reset();
        } else {
          data.assign(
            std::istreambuf_iterator<char>(stream->rdbuf()),
            std::istreambuf_iterator<char>());
        }
        stream.reset();
      } else if (output.get()) {
        *output << data;
        output.reset();
      }
    }

    void write() {
      if (!stream.get())
        input_stream(new std::istringstream(data)).move(stream);
    }

    keyword_type type;
    state_t state;
    std::string name;
    std::string mime;
    std::string data;
    input_stream stream;
    output_stream output;
  };

  struct keyword_index {
    std::string keyword;
    int index;

    keyword_index(std::string const &k, int i)
    : keyword(k), index(i)
    {}
  };

  inline std::size_t hash_value(keyword_index const &x) {
    std::size_t seed = 0;
    boost::hash_combine(seed, rest::utils::string_ihash()(x.keyword));
    boost::hash_combine(seed, x.index);
    return seed;
  }

  bool operator==(keyword_index const &x, keyword_index const &y) {
    return x.index == y.index && rest::utils::string_iequals()(x.keyword, y.keyword);
  }
}

class keywords::impl {
public:
  typedef boost::unordered_map<keyword_index, keyword_data> data_t;

  data_t data;

  data_t::iterator find(std::string const &keyword, int index) {
    data_t::iterator it = data.find(keyword_index(keyword, index));
    if (it == data.end()) {
      std::ostringstream x;
      x << "invalid keyword (" << keyword << ',' << index << ')';
      throw std::logic_error(x.str());
    }
    return it;
  }

  input_stream entity;
  std::string boundary;
  std::auto_ptr<io::filtering_istream> element;
  std::string next_name;
  std::string next_filename;
  std::string next_filetype;

  keyword_data *last;

  bool start_element() {
    if (last) {
      last->read();
      last = 0;
    }

    if (entity->peek() == EOF)
      return false;

    element.reset(new io::filtering_istream);

    element->push(utils::boundary_filter("\r\n" + boundary));
    element->push(boost::ref(*entity), 0, 0);

    return true;
  }

  void read_headers() {
    typedef
      boost::unordered_map<
        std::string, std::string,
        rest::utils::string_ihash,
        rest::utils::string_iequals
      > hm;

    hm headers;
    utils::http::read_headers(*element, headers);

    parse_content_disposition(headers["Content-Disposition"]);

    {
      hm::iterator it = headers.find("Content-Type");
      if (it != headers.end())
        next_filetype = it->second;
      else
        next_filetype = "application/octet-stream";
    }
  }

  void parse_content_disposition(std::string const &disp) {
    std::string type;
    std::set<std::string> interests;
    interests.insert("name");
    interests.insert("filename");
    std::map<std::string, std::string> param;
    utils::http::parse_parametrised(disp, type, interests, param);

    next_name = param["name"];
    next_filename = param["filename"];
  }

  void prepare_element(bool read) {
    data_t::iterator it = find_next_form(next_name);

    if (it == data.end()) {
      element->ignore(std::numeric_limits<int>::max());
      element.reset();
      return;
    }

    keyword_data &x = it->second;

    x.name = next_filename;
    x.mime = next_filetype;
    input_stream(element.release()).move(x.stream);
    x.state = keyword_data::s_prepared;

    if (read) {
      last = 0;
      x.read();
    } else {
      last = &x;
    }
  }

  bool read_until(std::string const &next) {
    if (!entity)
      return false;
    while (start_element()) {
      read_headers();
      if (next_name == next) {
        prepare_element(false);
        return true;
      }
      prepare_element(true);
    }
    entity.reset();
    return false;
  }

  void read_until(keyword_index const &next_index, keyword_data &next_data) {
    if (next_data.state != keyword_data::s_unset)
      return;
    if (!read_until(next_index.keyword))
      next_data.state = keyword_data::s_normal;
  }

  void unread_form() {
    for (data_t::iterator it = data.begin(); it != data.end(); ++it)
      if (it->second.type == FORM_PARAMETER)
        it->second.state = keyword_data::s_unset;
  }

  data_t::iterator find_next_form(std::string const &name) {
    data_t::iterator it;

    int i = 0;
    for (;;) {
      it = data.find(keyword_index(name, i++));
      if (it == data.end())
        break;
      if (it->second.type != FORM_PARAMETER)
        return data.end();
      if (it->second.state == keyword_data::s_unset)
        break;
    }

    if (it == data.end() && i > 1)
      it = data.insert(
          std::make_pair(keyword_index(name, i-1), keyword_data(FORM_PARAMETER))
        ).first;

    return it;
  }

  impl() : last(0) {}
};

keywords::keywords() : p(new impl) {
}

keywords::~keywords() {
}

bool keywords::exists(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->data.find(keyword_index(keyword, index));
  if (it == p->data.end()) {
    p->read_until(keyword);
    it = p->data.find(keyword_index(keyword, index));
  }
  return it != p->data.end();
}

std::string &keywords::access(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->find(keyword, index);
  p->read_until(it->first, it->second);
  it->second.read();
  return it->second.data;
}

bool keywords::is_set(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->find(keyword, index);
  return it->second.state != keyword_data::s_unset;
}

void keywords::declare(
    std::string const &keyword, int index, keyword_type type)
{
  impl::data_t::iterator it =
    p->data.insert(std::make_pair(
        keyword_index(keyword, index), keyword_type(type))
      ).first;

  if (it->second.type != type) {
    std::ostringstream x;
    x << "inconsistent keyword type (" << keyword << ',' << index << ')';
    throw std::logic_error(x.str());
  }
}

keyword_type keywords::get_declared_type(
    std::string const &keyword, int index) const
{
  impl::data_t::iterator it = p->data.find(keyword_index(keyword, index));
  if (it == p->data.end())
    return NONE;
  return it->second.type;
}

void keywords::set(
    std::string const &keyword, int index, std::string const &data)
{
  impl::data_t::iterator it = p->data.insert(std::make_pair(
      keyword_index(keyword, index), keyword_data()
    )).first;

  it->second.state = keyword_data::s_normal;
  it->second.data = data;
  it->second.stream.reset();
}

void keywords::set_with_type(
    keyword_type type,
    std::string const &keyword,
    int index,
    std::string const &data)
{
  impl::data_t::iterator it = p->data.find(keyword_index(keyword, index));
  if (it == p->data.end())
    return;
  if (it->second.type != type)
    return;
 
  it->second.state = keyword_data::s_normal;
  it->second.data = data;
  it->second.stream.reset();
}

void keywords::set_stream(
    std::string const &keyword, int index, input_stream &stream)
{
  impl::data_t::iterator it = p->data.insert(std::make_pair(
      keyword_index(keyword, index), keyword_data())
    ).first;

  it->second.state = keyword_data::s_normal;
  stream.move(it->second.stream);
  it->second.data.clear();
}

void keywords::set_name(
    std::string const &keyword, int index, std::string const &name)
{
  impl::data_t::iterator it = p->find(keyword, index);
  it->second.name = name;
}

void keywords::unset(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->find(keyword, index);
  keyword_data &x = it->second;
  x.state = keyword_data::s_unset;
  x.name.clear();
  x.mime.clear();
  x.data.clear();
  x.stream.reset();
  x.output.reset();
}

std::string keywords::get_name(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->find(keyword, index);
  return it->second.name;
}

std::istream &keywords::read(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->find(keyword, index);
  p->read_until(it->first, it->second);
  it->second.write();
  return *it->second.stream;
}

void keywords::set_entity(
    input_stream &entity, std::string const &content_type)
{
  std::string type;
  std::set<std::string> pset;
  pset.insert("boundary");
  std::map<std::string, std::string> params;

  utils::http::parse_parametrised(content_type, type, pset, params);

  if (type == "multipart/form-data") {
    entity.move(p->entity);

    p->boundary = "--" + params["boundary"];

    // Strip preamble and first boundary
    {
      io::filtering_istream filt;
      filt.push(utils::boundary_filter(p->boundary));
      filt.push(boost::ref(*p->entity), 0, 0);
      filt.ignore(std::numeric_limits<int>::max());
    }

    p->unread_form();
  } else if (type == "application/x-www-form-urlencoded") {
    std::string data;
    data.assign(
      std::istreambuf_iterator<char>(entity->rdbuf()),
      std::istreambuf_iterator<char>());

    add_uri_encoded(data);
  } else {
    for (impl::data_t::iterator it = p->data.begin();
        it != p->data.end();
        ++it)
    {
      keyword_data &x = it->second;
      if (x.type == ENTITY) {
        x.state = keyword_data::s_normal;
        x.data.clear();
        entity.move(x.stream);
        break;
      }
    }
    if (entity.get()) {
      entity->ignore(std::numeric_limits<int>::max());
      entity.reset();
    }
  }
}

void keywords::add_uri_encoded(std::string const &data) {
  p->unread_form();

  boost::char_separator<char> sep("&");

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  tokenizer pairs(data.begin(), data.end(), sep);

  for (tokenizer::iterator it = pairs.begin(); it != pairs.end(); ++it) {
    std::string::const_iterator split = std::find(it->begin(), it->end(), '=');
    std::string key = uri::unescape(it->begin(), split, true);

    impl::data_t::iterator el = p->find_next_form(key);

    if (el != p->data.end()) {
      if (split != it->end())
        ++split;
      keyword_data &x = el->second;
      x.state = keyword_data::s_prepared;
      x.stream.reset();
      x.data = uri::unescape(split, it->end(), true);
    }
  }
}

void keywords::set_request_data(request const &req) {
  headers const &h = req.get_headers();

  h.for_each_header(
    boost::bind(&keywords::set_with_type, this, HEADER, _1, _2));

  boost::optional<std::string> cookie_ = h.get_header("Cookie");
  if (cookie_) {
    std::vector<cookie> cookies;
    utils::http::parse_cookie_header(cookie_.get(), cookies);

    for (std::vector<cookie>::iterator it = cookies.begin();
        it != cookies.end();
        ++it)
      set_with_type(COOKIE, it->name, it->value);
  }
}

void keywords::set_output(
    std::string const &keyword, int index, output_stream &stream)
{
  impl::data_t::iterator it = p->find(keyword, index);
  stream.move(it->second.output);
}

void keywords::flush() {
  if (!p->entity)
    return;
  while (p->start_element()) {
    p->read_headers();
    p->prepare_element(true);
  }
  p->entity.reset();
}
