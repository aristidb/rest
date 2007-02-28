// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"

using namespace rest;

namespace {
  class global_responder : public responder<> {
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
}

class context::impl {
public:
  global_responder self_responder;
};

context::context() : p(new impl) {
}

context::~context() {
}

void context::declare_keyword(std::string const &, keyword_type) {
}

void context::do_bind(std::string const &, detail::responder_base &) {
}

void context::do_bind(
  std::string const &, detail::responder_base &, detail::any_path const &)
{
}

responder<> &context::get_responder() {
  return p->self_responder;
}
