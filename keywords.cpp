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

    void write() const {
      if (!stream.get())
        input_stream(new std::istringstream(data)).move(stream);
    }

    std::string keyword;
    int index;
    keyword_type type;
    mutable state_t state;
    mutable std::string name;
    mutable std::string mime;
    mutable std::string data;
    mutable input_stream stream;
    mutable output_stream output;
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
    data_t::iterator it = data.find(boost::make_tuple(keyword, index));
    if (it == data.end()) {
      std::ostringstream x;
      x << "invalid keyword (" << keyword << ',' << index << ')';
      throw std::logic_error(x.str());
    }
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

    it->name = next_filename;
    it->mime = next_filetype;
    input_stream(element.release()).move(it->stream);
    it->state = entry::s_prepared;

    if (read) {
      last = 0;
      it->read();
    } else {
      last = &*it;
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

  void read_until(entry const &next) {
    if (next.state != entry::s_unread)
      return;
    if (!read_until(next.keyword))
      next.state = entry::s_normal;
  }

  void unread_form() {
    for (data_t::iterator it = data.begin(); it != data.end(); ++it)
      if (it->type == FORM_PARAMETER)
        it->state = entry::s_unread;
  }

  data_t::iterator find_next_form(std::string const &name) {
    data_t::iterator it;

    int i = 0;
    for (;;) {
      it = data.find(boost::make_tuple(name, i++));
      if (it == data.end())
        break;
      if (it->type != FORM_PARAMETER)
        return data.end();
      if (it->state == entry::s_unread)
        break;
    }

    if (it == data.end() && i > 1)
      it = data.insert(entry(name, i - 1, FORM_PARAMETER)).first;

    return it;
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
  if (it->type != type) {
    std::ostringstream x;
    x << "inconsistent keyword type (" << keyword << ',' << index << ')';
    throw std::logic_error(x.str());
  }
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
    std::string const &keyword, int index, input_stream &stream)
{
  impl::data_t::iterator it = p->data.insert(impl::entry(keyword, index)).first;
  it->state = impl::entry::s_normal;
  stream.move(it->stream);
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
    std::auto_ptr<std::istream> &entity, std::string const &content_type)
{
  std::string type;
  std::set<std::string> pset;
  pset.insert("boundary");
  std::map<std::string, std::string> params;

  utils::http::parse_parametrised(content_type, type, pset, params);

  if (type == "multipart/form-data") {
    p->entity.reset(entity.release());

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
    declare("_", NORMAL);
    set_stream("_", input_stream(entity.release()));
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
      el->state = impl::entry::s_prepared;
      el->stream.reset();
      el->data = uri::unescape(split, it->end(), true);
    }
  }
}

void keywords::set_header_fields(std::map<std::string, std::string> const &f) {
  typedef std::map<std::string, std::string>::const_iterator iterator;
  iterator const end = f.end();
  for(iterator i = f.begin(); i != end; ++i) {
    if(i->first == "Cookie" || i->first == "Cookie2")
        // TODO handle Cookies (see RFC 2965)
      ;

    impl::data_t::iterator x = p->data.find(boost::make_tuple(i->first, 0));
    if (x != p->data.end() && x->type == HEADER) {
      x->state = impl::entry::s_normal;
      x->data = i->second;
      x->stream.reset();
    }
  }
}

void keywords::set_output(
    std::string const &keyword, int index, output_stream &stream)
{
  impl::data_t::iterator it = p->find(keyword, index);
  stream.move(it->output);
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
