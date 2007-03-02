// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <stdexcept>

using namespace rest;
using namespace boost::multi_index;

namespace {
  struct global_responder : public responder<> {
    response get(std::string const &, keywords const &) {
      return 404;
    }

    response put(std::string const &, keywords const &) {
      return 404;
    }

    response post(std::string const &, keywords const &) {
      return 404;
    }

    response delete_(std::string const &, keywords const &) {
      return 404;
    }
  };

  struct keyword_info {
    keyword_info(std::string const &keyword, keyword_type type)
    : keyword(keyword), type(type) {}

    std::string keyword;
    keyword_type type;
  };

  typedef boost::multi_index_container<
    keyword_info,
    indexed_by<
      hashed_unique<
        member<keyword_info, std::string, &keyword_info::keyword>
      >
    >
  > keyword_info_set;
}

class context::impl {
public:
  global_responder self_responder;
  keyword_info_set predeclared_keywords;
};

context::context() : p(new impl) {
}

context::~context() {
}

void context::declare_keyword(std::string const &keyword, keyword_type type) {
  bool x = p->predeclared_keywords.insert(keyword_info(keyword, type)).second;
  if (!x)
    throw std::logic_error("keyword already declared");
}

void context::do_bind(
  std::string const &, detail::responder_base &, detail::any_path const &)
{
}

responder<> &context::get_responder() {
  return p->self_responder;
}
