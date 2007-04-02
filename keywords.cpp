// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/algorithm/string.hpp>
#include <stdexcept>
#include <sstream>
#include <iostream>//DEBUG

using namespace rest;
using namespace boost::multi_index;
namespace algo = boost::algorithm;

class keywords::impl {
public:
  struct entry {
    entry(std::string const &keyword, int index, keyword_type type = NORMAL)
    : keyword(keyword), index(index), type(type), pending_read(false) {}

    entry(entry const &o)
    : keyword(o.keyword), index(o.index), type(o.type), pending_read(false) {}

    void read() const {
      if (!pending_read) return;
      std::ostringstream out;
      out << stream->rdbuf();
      data = out.str();
      pending_read = false;
    }

    std::string keyword;
    int index;
    keyword_type type;
    mutable bool pending_read;
    mutable std::string name;
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

  boost::scoped_ptr<std::istream> entity;

  data_t::iterator find(std::string const &keyword, int index) {
    data_t::iterator it = data.find(boost::make_tuple(keyword, index));
    if (it == data.end())
      throw std::logic_error("invalid keyword");
    return it;
  }
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
  it->pending_read = false;
  it->stream.reset();
}

void keywords::set_stream(
    std::string const &keyword, int index, std::istream *stream)
{
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword, index)).first;
  it->pending_read = true;
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
  std::cout << "~~ " << content_type << std::endl;

  typedef std::string::const_iterator iterator;

  iterator begin = content_type.begin();
  iterator end = content_type.end();

  iterator param_left = std::find(begin, end, ';');
  iterator param_right = param_left;
  if (param_right != end) {
    while (++param_right != end)
      if (*param_right != ' ')
        break;
  }

  std::string type(begin, param_left);
  std::string rest(param_right, end);

  std::cout << "~~ " << type << " ; " << rest << std::endl;

  p->entity.reset(entity);
}

void keywords::set_output(
    std::string const &keyword, int index, std::ostream *stream)
{
  impl::data_t::iterator it = p->find(keyword, index);
  it->output.reset(stream);
}

void keywords::flush() {
}
