// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"

using namespace rest;

class context::impl {
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
}
