// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/context.hpp>
#include <rest/utils/uri.hpp>
#include <rest/utils/string.hpp>
#include <rest/config.hpp>
#include <boost/unordered_map.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/tokenizer.hpp>
#include <stdexcept>
#include <algorithm>

using namespace rest;
namespace det = rest::detail;
using namespace boost::multi_index;
namespace uri = rest::utils::uri;

class context::impl {
public:
  typedef boost::unordered_map<
      std::string, keyword_type,
      rest::utils::string_ihash,
      rest::utils::string_iequals
    > keyword_type_map_type;

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

    path_resolver_node *child(std::string const &x) const {
      conditional_children_t::iterator it = conditional_children.find(x);
      if (it != conditional_children.end())
        return *it;
      return unconditional_child.get();
    }

    void attach(server &srv) {
      if (responder_)
        responder_->attach(srv);
      if (context_)
        context_->attach(srv);

      if (unconditional_child)
        unconditional_child->attach(srv);

      for (conditional_children_t::iterator it = conditional_children.begin();
          it != conditional_children.end();
          ++it)
        (*it)->attach(srv);
    }
  };

  keyword_type_map_type predeclared_keywords;
  path_resolver_node root;

  path_resolver_node *make_bindable(std::string const &spec);
};

context::context() : p(new impl) {
}

context::~context() { }

void context::declare_keyword(std::string const &keyword, keyword_type type) {
  bool x =
    p->predeclared_keywords.insert(std::make_pair(keyword, type)).second;
  if (!x)
    throw std::logic_error("keyword already declared");
}

keyword_type context::get_keyword_type(std::string const &keyword) const {
  impl::keyword_type_map_type::iterator it =
    p->predeclared_keywords.find(keyword);
  if (it == p->predeclared_keywords.end())
    return NONE;
  return it->second;
}

void context::enum_keywords(
  keyword_type type,
  boost::function<void (std::string const &)> const &callback) const
{
  for (impl::keyword_type_map_type::iterator it = p->predeclared_keywords.begin();
      it != p->predeclared_keywords.end();
      ++it)
    if (type == NONE || it->second == type)
      callback(it->first);
}

void context::prepare_keywords(keywords &kw) const {
  for (impl::keyword_type_map_type::iterator it = p->predeclared_keywords.begin();
      it != p->predeclared_keywords.end();
      ++it)
    kw.declare(it->first, it->second);
}

#ifndef NDEBUG
#define CHECK_PATH_LEN(path) {                                          \
    std::size_t const n = rest::utils::get(rest::config::get().tree(),  \
                                           0ul,                         \
                                           "general", "uri_length");    \
    if(n != 0)                                                          \
      assert(path.size() > n);                                          \
  } while(false)                                                        \
  /* */
#else
#define CHECK_PATH_LEN(path) while(false)
#endif

void context::do_bind(
  std::string const &path,
  det::responder_base &responder_,
  det::any_path const &associated)
{
  CHECK_PATH_LEN(path);
  impl::path_resolver_node *current = p->make_bindable(path);
  current->responder_ = &responder_;
  current->associated_path_id = associated;
}

void context::do_bind(
  std::string const &path,
  context &context_,
  det::any_path const &associated)
{
  CHECK_PATH_LEN(path);
  impl::path_resolver_node *current = p->make_bindable(path);
  current->context_ = &context_;
  current->associated_path_id = associated;
}

#undef CHECK_PATH_LEN

void context::find_responder(
  std::string const &path,
  det::any_path &path_id,
  det::responder_base *&out_responder,
  context *&out_context,
  keywords &out_keywords)
{
  out_responder = 0;

  typedef std::string::const_iterator iterator;

  iterator start = path.begin();
  iterator end = path.end();
  iterator middle = std::find(start, end, '?');

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

  {
    boost::char_separator<char> sep("/", "=", boost::keep_empty_tokens);
    tokenizer tokens(start, middle, sep);

    tokenizer::iterator start = tokens.begin();
    while (start != tokens.end() && start->empty())
      ++start;

    do_find_responder(
      start, tokens.end(),
      path_id, out_responder, out_context, out_keywords);
  }

  out_context->prepare_keywords(out_keywords);

  if (middle != end)
    out_keywords.add_uri_encoded(std::string(++middle, end));
}

template<class Iterator>
void context::do_find_responder(
  Iterator it, Iterator end,
  det::any_path &path_id,
  det::responder_base *&out_responder,
  context *&out_context,
  keywords &out_keywords)
{
  out_context = this;

  impl::path_resolver_node *current = &p->root;

  std::string last;

  for (; !current->ellipsis && it != end; ++it) {
    std::string text = uri::unescape(*it, false);
    current = current->child(text);
    if (!current)
      return;
    if (current->type == impl::path_resolver_node::closure) {
      out_keywords.declare(current->data, NORMAL);
      out_keywords.set(current->data, text);
    } else if (!current->ellipsis) {
      last = text;
    }
 }

  if (current->responder_) {
    if (!current->associated_path_id.empty()) {
      path_id = current->associated_path_id;
    } else {
      if (current->ellipsis) {
        last.reserve(it->size() + (it.end() - it.base()));
        last.assign(*it);
        last.append(it.base(), it.end());
      }
      path_id = det::any_path(last);
    }
    out_responder = current->responder_;
  } else if (current->context_) {
    current->context_->do_find_responder(
        it, end,
        path_id, out_responder, out_context, out_keywords);
  }
}

context::impl::path_resolver_node *
context::impl::make_bindable(std::string const &path) {
  impl::path_resolver_node *current = &root;

  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  boost::char_separator<char> sep("/", "=", boost::keep_empty_tokens);
  tokenizer tokens(path, sep);

  tokenizer::iterator it = tokens.begin();
  while (it != tokens.end() && it->empty())
    ++it;

  for (; it != tokens.end(); ++it) {
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
      std::string text = uri::unescape(*it, false);
      path_resolver_node::conditional_children_t::iterator i_next =
        current->conditional_children.find(text);
      if (i_next == current->conditional_children.end()) {
        path_resolver_node *next = new path_resolver_node;
        next->type = path_resolver_node::literal;
        next->data = text;
        i_next = current->conditional_children.insert(next).first;
      }
      current = *i_next;
    }
  }
  return current;
}

void context::attach(server &srv) {
  p->root.attach(srv);
}

// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
