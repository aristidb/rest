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
#include <iostream>//DEBUG

using namespace rest;
using namespace boost::multi_index;
namespace io = boost::iostreams;

class keywords::impl {
public:
  struct entry {
    entry(std::string const &keyword, int index, keyword_type type = NORMAL)
    : keyword(keyword), index(index), type(type), pending_read(false) {}

    entry(entry const &o)
    : keyword(o.keyword), index(o.index), type(o.type), pending_read(false) {}

    void read() const {
      if (!pending_read) return;
      data.assign(
        std::istreambuf_iterator<char>(stream->rdbuf()),
        std::istreambuf_iterator<char>());
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
  std::string boundary;

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
  p->entity.reset(entity);

/*
mal die BNF suchen und hier rein schreiben:

------------------
     boundary := 0*69<bchars> bcharsnospace

     bchars := bcharsnospace / " "

     bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
                      "+" / "_" / "," / "-" / "." /
                      "/" / ":" / "=" / "?"

   Overall, the body of a "multipart" entity may be specified as
   follows:

     dash-boundary := "--" boundary
                      ; boundary taken from the value of
                      ; boundary parameter of the
                      ; Content-Type field.

     multipart-body := [preamble CRLF]
                       dash-boundary transport-padding CRLF
                       body-part *encapsulation
                       close-delimiter transport-padding
                       [CRLF epilogue]

     transport-padding := *LWSP-char
                          ; Composers MUST NOT generate
                          ; non-zero length transport
                          ; padding, but receivers MUST
                          ; be able to handle padding
                          ; added by message transports.

     encapsulation := delimiter transport-padding
                      CRLF body-part

     delimiter := CRLF dash-boundary

     close-delimiter := delimiter "--"

     preamble := discard-text

     epilogue := discard-text

     discard-text := *(*text CRLF) *text
                     ; May be ignored or discarded.

     body-part := MIME-part-headers [CRLF *OCTET]
                  ; Lines in a body-part must not start
                  ; with the specified dash-boundary and
                  ; the delimiter must not appear anywhere
                  ; in the body part.  Note that the
                  ; semantics of a body-part differ from
                  ; the semantics of a message, as
                  ; described in the text.

     OCTET := <any 0-255 octet value>
------------------
*/

/*
also, wie sieht das denn dann so grob aus?

[bla\r\n]
--boundary                 \r\n
Header\r\n
Header\r\n
[\r\n
Daten]
\r\n--boundary             \r\n
Header\r\n
[\r\n
Daten]
\r\n--boundary--            [\r\n
bla]
*/

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
    filt.pop();
  }

  while (entity->peek() != EOF) {
    io::filtering_istream filt;
    filt.push(utils::boundary_filter("\r\n" + p->boundary));
    filt.push(boost::ref(*entity), 0, 0);

    std::cout << "<<\n";
    char ch;
    while (filt.get(ch))
      if (ch == 0)
        std::cout << "\\0";
      else if (ch == '\r')
        std::cout << "\\r";
      else if (ch == '\n')
        std::cout << "\\n\n";
      else
        std::cout << ch;
    std::cout << "\n>>\n" << std::flush;

    filt.pop();
  }
}

void keywords::set_output(
    std::string const &keyword, int index, std::ostream *stream)
{
  impl::data_t::iterator it = p->find(keyword, index);
  it->output.reset(stream);
}

void keywords::flush() {
}
