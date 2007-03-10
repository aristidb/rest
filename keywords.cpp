// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <stdexcept>
#include <sstream>

using namespace rest;
using namespace boost::multi_index;

class keywords::impl {
public:
  struct entry {
    entry(std::string const &keyword)
    : keyword(keyword), pending_read(false) {}
    entry(entry const &o) : keyword(o.keyword) {} // dirty, yeah

    void read() const {
      if (!pending_read) return;
      std::ostringstream out;
      out << stream->rdbuf();
      data = out.str();
      pending_read = false;
    }

    std::string keyword;
    mutable bool pending_read;
    mutable std::string name;
    mutable std::string data;
    mutable boost::scoped_ptr<std::istream> stream;
  };

  typedef
  boost::multi_index_container<
    entry,
    indexed_by<
      hashed_unique<
        member<entry, std::string, &entry::keyword>
      >
    >
  > data_t;

  data_t data;
};

keywords::keywords() : p(new impl) {
}

keywords::~keywords() {
}

std::string &keywords::operator[](std::string const &keyword) {
  impl::data_t::iterator it = p->data.find(keyword);
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  it->read();
  return it->data;
}

void keywords::set(std::string const &keyword, std::string const &data) {
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword)).first;
  it->data = data;
  it->pending_read = false;
  it->stream.reset();
}

void keywords::set_stream(std::string const &keyword, std::istream *stream) {
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword)).first;
  it->pending_read = true;
  it->stream.reset(stream);
}

void keywords::set_name(std::string const &keyword, std::string const &name) {
  impl::data_t::iterator it = p->data.find(keyword);
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  it->name = name;
}

std::string keywords::get_name(std::string const &keyword) const {
  impl::data_t::iterator it = p->data.find(keyword);
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  return it->name;
}

std::istream &keywords::read(std::string const &keyword) {
  impl::data_t::iterator it = p->data.find(keyword);
  if (it == p->data.end())
    throw std::logic_error("invalid keyword");
  if (!it->stream)
    it->stream.reset(new std::istringstream(it->data));
  return *it->stream;
}
