// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include "rest-utils.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/tuple/tuple.hpp>
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
namespace io = boost::iostreams;

class keywords::impl {
public:
  struct entry {
    entry(std::string const &keyword, int index, keyword_type type = NORMAL)
    : keyword(keyword), index(index), type(type) {}

    entry(entry const &o)
    : keyword(o.keyword), index(o.index), type(o.type) {}

    void read() const {
      std::cout << "READ KEYWORD " << keyword << ',' << index << std::endl;
      if (stream) {
        if (output) {
          *output << stream->rdbuf();
        } else {
          data.assign(
            std::istreambuf_iterator<char>(stream->rdbuf()),
            std::istreambuf_iterator<char>());
        }
        stream.reset();
      } else if (output) {
        *output << data;
      }
    }

    std::string keyword;
    int index;
    keyword_type type;
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
  entry const *next;

  bool start_element() {
    if (entity->peek() == EOF)
      return false;

    element.reset(new io::filtering_istream);

    element->push(utils::boundary_filter("\r\n" + boundary));
    element->push(boost::ref(*entity), 0, 0);

    return true;
  }

  void read_headers() {
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

    for (header_fields::iterator it = headers.begin(); it != headers.end();++it)
      std::cout << "_ " << it->first << ": " << it->second << std::endl;

    std::cout << "__ name: " << next_name << std::endl;
    std::cout << "__ filename: " << next_filename << std::endl;
    std::cout << "__ filetype: " << next_filetype << std::endl;

    {
      data_t::iterator it = data.find(boost::make_tuple(next_name, 0));
      if (it == data.end() || it->type != FORM_PARAMETER) {
        element->ignore(std::numeric_limits<int>::max());
        element.reset();
        next = 0;
        return;
      }
      it->name = next_filename;
      it->mime = next_filetype;
      it->stream.reset(element.release());
      next = &*it;
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

  void read_element() {
    if (next)
      next->read();
  }

  impl() : next(0) {}
};

keywords::keywords() : p(new impl) {
}

keywords::~keywords() {
}

bool keywords::exists(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  return it != p->data.end();
}

std::string &keywords::access(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->find(keyword, index);
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
  it->data = data;
  it->stream.reset();
}

void keywords::set_stream(
    std::string const &keyword, int index, std::istream *stream)
{
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword, index)).first;
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
  if (!it->stream)
    it->stream.reset(new std::istringstream(it->data));
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

  while (p->start_element()) {
    p->read_headers();
    p->read_element();
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
  p->entity.reset();
}
