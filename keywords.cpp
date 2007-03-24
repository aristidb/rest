// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/tuple/tuple.hpp>
#include <stdexcept>
#include <sstream>

using namespace rest;
using namespace boost::multi_index;

class keywords::impl {
public:
  struct entry {
    entry(std::string const &keyword, int index)
    : keyword(keyword), index(index), pending_read(false) {}
    entry(entry const &o) : keyword(o.keyword), index(o.index) {} // dirty, yeah

    void read() const {
      if (!pending_read) return;
      std::ostringstream out;
      out << stream->rdbuf();
      data = out.str();
      pending_read = false;
    }

    std::string keyword;
    int index;
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
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  it->read();
  return it->data;
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
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  it->name = name;
}

std::string keywords::get_name(std::string const &keyword, int index) const {
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  return it->name;
}

std::istream &keywords::read(std::string const &keyword, int index) {
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  if (!it->stream)
    it->stream.reset(new std::istringstream(it->data));
  return *it->stream;
}

void keywords::set_entity(std::istream *entity) {
  p->entity.reset(entity);
}

void keywords::set_output(
    std::string const &keyword, int index, std::ostream *stream)
{
  impl::data_t::iterator it = p->data.find(boost::make_tuple(keyword, index));
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  it->output.reset(stream);
}

void keywords::flush() {
}
