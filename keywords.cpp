// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tokenizer.hpp>
#include <stdexcept>
#include <sstream>
#include <map>
#include <set>
#include <limits>
#include <cstdio>
#include <cctype>
#include <memory>
#include <iostream>//DEBUG

using namespace rest;
using namespace boost::multi_index;
namespace uri = rest::utils::uri;
namespace io = boost::iostreams;

class keywords::impl {
public:
  struct entry {
    enum state_t { s_normal, s_unread, s_prepared };

    entry(std::string const &keyword, int index, keyword_type type = NORMAL)
    : keyword(keyword), index(index), type(type), state(s_normal) {}

    entry(entry const &o)
    : keyword(o.keyword), index(o.index), type(o.type), state(s_normal) {}

    void read() const {
      std::cout << "read(e) " << keyword << ',' << index << std::endl;
      if (stream) {
        std::cout << "\tstream" << std::endl;
        if (output) {
          *output << stream->rdbuf();
          output.reset();
        } else {
          data.assign(
            std::istreambuf_iterator<char>(stream->rdbuf()),
            std::istreambuf_iterator<char>());
        }
        stream.reset();
      } else if (output) {
        *output << data;
        output.reset();
      }
    }

    void write() const {
      std::cout << "write(e) " << keyword << ',' << index << std::endl;
      if (!stream)
        stream.reset(new std::istringstream(data));
    }

    std::string keyword;
    int index;
    keyword_type type;
    mutable state_t state;
    mutable std::string name;
    mutable std::string mime;
    mutable std::string data;
    mutable boost::scoped_ptr<std::istream> stream;
    mutable boost::scoped_ptr<std::ostream> output;
  };

  typedef
  boost::multi_index_container<
    entry,
    indexed_by<
      hashed_unique<
        composite_key<
          entry,
          member<entry, std::string, &entry::keyword>,
          member<entry, int, &entry::index>
        >
      >
    >
  > data_t;

  data_t data;

  data_t::iterator find(std::string const &keyword, int index) {
    std::cout << "FIND KEYWORD " << keyword << ',' << index << std::endl;
    data_t::iterator it = data.find(boost::make_tuple(keyword, index));
    if (it == data.end())
      throw std::logic_error("invalid keyword");
    return it;
  }

  boost::scoped_ptr<std::istream> entity;
  std::string boundary;
  std::auto_ptr<io::filtering_istream> element;
  std::string next_name;
  std::string next_filename;
  std::string next_filetype;

  entry const *last;

  bool start_element() {
    if (last) {
      last->read();
      last = 0;
    }

    std::cout << "start_element" << std::endl;

    if (entity->peek() == EOF)
      return false;

    element.reset(new io::filtering_istream);

    element->push(utils::boundary_filter("\r\n" + boundary));
    element->push(boost::ref(*entity), 0, 0);

    return true;
  }

  void read_headers() {
    std::cout << "read_headers" << std::endl;
    using utils::http::header_fields;

    header_fields headers = utils::http::read_headers(*element);

    parse_content_disposition(headers["content-disposition"]);

    {
      header_fields::iterator it = headers.find("content-type");
      if (it != headers.end())
        next_filetype = it->second;
      else
        next_filetype = "application/octet-stream";
    }

    std::cout << "\tname: " << next_name << std::endl;
    std::cout << "\tfilename: " << next_filename << std::endl;
    std::cout << "\tfiletype: " << next_filetype << std::endl;
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
    std::cout << "prepare_element " << read << std::endl;
    data_t::iterator it;
    int i = 0;
    for (;;) {
      it = data.find(boost::make_tuple(next_name, i++));
      if (it == data.end())
        break;
      if (it->type != FORM_PARAMETER)
        break;
      if (it->state == entry::s_unread)
        break;
    }

    if (it == data.end() && i > 1)
      it = data.insert(entry(next_name, i - 1, FORM_PARAMETER)).first;

    if (it == data.end() || it->type != FORM_PARAMETER) {
      element->ignore(std::numeric_limits<int>::max());
      element.reset();
      return;
    }

    it->name = next_filename;
    it->mime = next_filetype;
    it->stream.reset(element.release());
    it->state = entry::s_prepared;

    if (read) {
      last = 0;
      it->read();
    } else {
      last = &*it;
    }
  }

  bool read_until(std::string const &next) {
    std::cout << "read until: " << next << std::endl;
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

  void read_until(entry const &next) {
    if (next.state != entry::s_unread)
      return;
    if (!read_until(next.keyword))
      next.state = entry::s_normal;
  }

  impl() : last(0) {}
};

keywords::keywords() : p(new impl) {
}

keywords::~keywords() {
}

bool keywords::exists(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end()) {
    p->read_until(keyword);
    it = p->data.find(boost::make_tuple(keyword, index));
  }
  return it != p->data.end();
}

std::string &keywords::access(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->find(keyword, index);
  p->read_until(*it);
  it->read();
  return it->data;
}

void keywords::declare(
    std::string const &keyword, int index, keyword_type type)
{
  impl::data_t::iterator it =
    p->data.insert(impl::entry(keyword, index, type)).first;
  if (it->type != type)
    throw std::logic_error("inconsistent type");
}

keyword_type keywords::get_declared_type(
    std::string const &keyword, int index) const
{
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end())
    return NONE;
  return it->type;
}

void keywords::set(
    std::string const &keyword, int index, std::string const &data)
{
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword, index)).first;
  it->state = impl::entry::s_normal;
  it->data = data;
  it->stream.reset();
}

void keywords::set_stream(
    std::string const &keyword, int index, std::istream *stream)
{
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword, index)).first;
  it->state = impl::entry::s_normal;
  it->stream.reset(stream);
}

void keywords::set_name(
    std::string const &keyword, int index, std::string const &name)
{
  impl::data_t::iterator it = p->find(keyword, index);
  it->name = name;
}

std::string keywords::get_name(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->find(keyword, index);
  return it->name;
}

std::istream &keywords::read(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->find(keyword, index);
  p->read_until(*it);
  it->write();
  return *it->stream;
}

void keywords::set_entity(
    std::istream *entity, std::string const &content_type)
{
  p->entity.reset(entity);

  std::cout << "~~ " << content_type << std::endl;

  std::string type;
  std::set<std::string> pset;
  pset.insert("boundary");
  std::map<std::string, std::string> params;

  utils::http::parse_parametrised(content_type, type, pset, params);
  std::cout << "~~ " << type << " ; " << params["boundary"] << std::endl;

  p->boundary = "--" + params["boundary"];

  // Strip preamble and first boundary
  {
    io::filtering_istream filt;
    filt.push(utils::boundary_filter(p->boundary));
    filt.push(boost::ref(*entity), 0, 0);
    filt.ignore(std::numeric_limits<int>::max());
  }

  for (impl::data_t::iterator it = p->data.begin(); it != p->data.end(); ++it)
    if (it->type == FORM_PARAMETER)
      it->state = impl::entry::s_unread;
}

void keywords::add_uri_encoded(std::string const &data) {
  boost::char_separator<char> sep("&");

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  tokenizer pairs(data.begin(), data.end(), sep);

  for (tokenizer::iterator it = pairs.begin(); it != pairs.end(); ++it) {
    std::string::const_iterator split = std::find(it->begin(), it->end(), '=');
    std::string key = uri::unescape(it->begin(), split, true);
    if (get_declared_type(key) == FORM_PARAMETER) {
      if (split != it->end())
        ++split;
      std::string value = uri::unescape(split, it->end(), true);
      set(key, value);
    }
  }
}

void keywords::set_output(
    std::string const &keyword, int index, std::ostream *stream)
{
  impl::data_t::iterator it = p->find(keyword, index);
  it->output.reset(stream);
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
