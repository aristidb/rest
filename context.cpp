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
    enum type_t { root, closure, literal };

    type_t type;
    std::string data;
    bool ellipsis;

    context *context_;
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

    path_resolver_node()
    : type(root), ellipsis(false), context_(0), responder_(0) {}

    ~path_resolver_node() {
      for (conditional_children_t::iterator it = conditional_children.begin();
          it != conditional_children.end();
          ++it)
        delete *it;
    }

    void print(int level) {
      for (int i = 0; i < level; ++i)
        std::cout << "  ";
      switch (type) {
      case root: std::cout << "</> "; break;
      case closure: std::cout << "<C> "; break;
      case literal: std::cout << "<L> "; break;
      };
      std::cout << data << (ellipsis ? "...\n" : "\n");
      if (unconditional_child) {
        for (int i = 0; i < level; ++i)
          std::cout << "  ";
        std::cout << " unconditional child:\n";
        unconditional_child->print(level + 1);
      }
      if (!conditional_children.empty()) {
        for (int i = 0; i < level; ++i)
          std::cout << "  ";
        std::cout << " conditional children:\n";
        for (conditional_children_t::iterator it = conditional_children.begin();
            it != conditional_children.end();
            ++it)
          (*it)->print(level + 1);
      }
    }
  };
}

class context::impl {
public:
  keyword_info_set predeclared_keywords;
  path_resolver_node root;

  path_resolver_node *make_bindable(std::string const &spec);
};

context::context() : p(new impl) {
}

context::~context() {
  std::cout << this << ":\n";
  p->root.print(1);
}

void context::declare_keyword(std::string const &keyword, keyword_type type) {
  bool x = p->predeclared_keywords.insert(keyword_info(keyword, type)).second;
  if (!x)
    throw std::logic_error("keyword already declared");
}

keyword_type context::get_keyword_type(std::string const &keyword) const {
  keyword_info_set::iterator it = p->predeclared_keywords.find(keyword);
  if (it == p->predeclared_keywords.end())
    return NONE;
  return it->type;
}

void context::do_bind(
  std::string const &path,
  det::responder_base &responder_,
  det::any_path const &associated)
{
  path_resolver_node *current = p->make_bindable(path);
  current->responder_ = &responder_;
  current->associated_path_id = associated;
}

void context::do_bind(
  std::string const &path,
  context &context_,
  det::any_path const &associated)
{
  path_resolver_node *current = p->make_bindable(path);
  current->context_ = &context_;
  current->associated_path_id = associated;
}

void context::find_responder(
  std::string const &path,
  det::any_path &path_id,
  det::responder_base *&out_responder,
  context *&out_context,
  keywords &out_keywords)
{
  // search... this is DUMMY
  out_responder = 0;
  out_context = this;
}

path_resolver_node *context::impl::make_bindable(std::string const &path) {
  path_resolver_node *current = &root;

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("/=");
  tokenizer tokens(path, sep);

  for (tokenizer::iterator it = tokens.begin(); it != tokens.end(); ++it) {
    if (*it == "...") { // ellipsis
      if (++it != tokens.end())
        throw std::logic_error("ellipsis (...) not at the end");
      current->ellipsis = true;
      break;
    } else if ((*it)[0] == '{') { // closure
      if (it->find('}') != it->length() - 1)
        throw std::logic_error("invalid closure");
      if (it->find('{', 1) != tokenizer::value_type::npos)
        throw std::logic_error("invalid closure");
      if (!current->unconditional_child)
        current->unconditional_child.reset(new path_resolver_node);
      current = current->unconditional_child.get();
      current->type = path_resolver_node::closure;
      current->data.assign(it->begin() + 1, it->end() - 1);
    } else { // literal
      if (it->find_first_of("{}") != tokenizer::value_type::npos)
        throw std::logic_error("only full closures are allowed");
      path_resolver_node::conditional_children_t::iterator i_next =
        current->conditional_children.find(*it);
      if (i_next == current->conditional_children.end()) {
        path_resolver_node *next = new path_resolver_node;
        next->type = path_resolver_node::literal;
        next->data = *it;
        i_next = current->conditional_children.insert(next).first;
      }
      current = *i_next;
    }
  }
  return current;
}
