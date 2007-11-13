// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/context.hpp"
#include "rest/rest.hpp"
#include "rest/input_stream.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <boost/iostreams/stream.hpp>

struct welcomer : rest::responder<rest::GET, rest::NO_PATH> {
  rest::response get(rest::keywords &data, rest::request const &) {
    rest::response response("text/plain");
    response.set_data(std::string("Welcome, ") + data["user"]);
    return response;
  }
};

struct displayer : rest::responder<rest::GET | rest::PUT> {
  rest::response
  get(std::string const &path, rest::keywords &data, rest::request const &) {
    if (path == "list") {
      // return a list of objects
      rest::response ok;
      return ok;
    } else {
      std::string id = data["id"];
      // display the object
      rest::response ok;
      return ok;
    }
  }
  rest::response
  put(std::string const &path, rest::keywords &, rest::request const &) {
    if (path == "list") {
      // error
      rest::response err(404);
      return err;
    } else {
      // upload the new object
      rest::response ok(201);
      return ok;
    }
  }
};

enum mode { COOKIE, KEYWORD, KEYWORD_NOT };

struct searcher : rest::responder<rest::GET, mode> {
  rest::response
  get(mode path, rest::keywords &, rest::request const &) {
    rest::response response("text/plain");
    if (path == COOKIE)
      response.set_data("Sorry, got no cookie thing!");
    else
      response.set_data("Am I your searching dork? Hell...");
    return response;
  }
};

struct rel : rest::responder<rest::GET> {
  rest::response
  get(std::string const &path, rest::keywords &, rest::request const &) {
    rest::response response("text/plain");
    response.set_data(path);
    return response;
  }
};

int main() {
  rest::context context;
  context.declare_keyword("user", rest::COOKIE);
  context.declare_keyword("xyz", rest::FORM_PARAMETER);

  welcomer welcome;
  context.bind("/", welcome);
  displayer display;
  context.bind("/list", display);
  context.bind("/%20 +/object/{id}", display);
  rest::context search;
  searcher search_obj;
  search.bind("/cookie", search_obj, COOKIE);
  search.bind("/keyword/{keyword}", search_obj, KEYWORD);
  search.bind("/keyword/not={keyword}", search_obj, KEYWORD_NOT);
  context.bind("/search/...", search);
  rel r;
  context.bind("/foo/...", r);

  rest::keywords kw;
  rest::detail::any_path path_id;
  rest::detail::responder_base *responder;
  rest::context *local;
  context.find_responder(
      "/ %20%2B/object/17?xyz=b+la",
      path_id, responder, local, kw);

  std::cout << responder << '/' << local << std::endl;
  std::cout << ':' << kw["id"] << std::endl;
  std::cout << path_id.type().name() << std::endl;
  std::cout << boost::any_cast<std::string>(path_id) << std::endl << std::endl;
  std::cout << "xyz=" << kw["xyz"] << std::endl;

  std::istringstream stream("test");
  kw.set_stream("other", rest::input_stream(stream));
  std::cout << "$$ " << kw["other"] << std::endl;

  context.find_responder("/foo/bar/bum=xy", path_id, responder, local, kw);
  std::cout << "/foo: " << boost::any_cast<std::string>(path_id) << std::endl;
}
