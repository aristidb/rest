// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/tokenizer.hpp>
#include <stdexcept>
#include <iostream>//FIXME

using namespace rest;
namespace det = rest::detail;
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

  struct path_resolver_node {
    enum type_t { literal, keyword };

    type_t type;
    std::string data;
    bool ellipsis;

    det::responder_base *responder_;
    det::any_path associated_path_id;

    boost::scoped_ptr<path_resolver_node> unconditional_child;
    typedef
      boost::multi_index_container<
        path_resolver_node *,
        indexed_by<
          hashed_unique<
            member<path_resolver_node, std::string, &path_resolver_node::data>
          >
        >
      > conditional_children_t;
    conditional_children_t conditional_children;

    ~path_resolver_node() {
      for (conditional_children_t::iterator it = conditional_children.begin();
          it != conditional_children.end();
          ++it)
        delete *it;
    }
  };
}

class context::impl {
public:
  global_responder self_responder;
  keyword_info_set predeclared_keywords;
  path_resolver_node root;
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
  std::string const &path,
  detail::responder_base &responder_,
  detail::any_path const &associated)
{
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("/=");
  std::cout << ':';
  tokenizer tokens(path, sep);
  for (tokenizer::iterator it = tokens.begin();
      it != tokens.end();
      ++it) {
    std::cout << '<' << *it << "> ";
    if (*it == "...") {
      if (++it != tokens.end())
        throw std::logic_error("ellipsis (...) not at the end");
      std::cout << "E ";
      // ellipsis at the end
      break;
    } else if ((*it)[0] == '{') {
      if (it->find('}') != it->length() - 1)
        throw std::logic_error("invalid closure");
      if (it->find('{', 1) != tokenizer::value_type::npos)
        throw std::logic_error("invalid closure");
      std::cout << "C ";
      // closure
    } else {
      if (it->find_first_of("{}") != tokenizer::value_type::npos)
        throw std::logic_error("only full closures are allowed");
      std::cout << "K ";
      // keyword
    }
  }
  std::cout << std::endl;
}

responder<> &context::get_responder() {
  return p->self_responder;
}
