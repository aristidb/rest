// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest.hpp"
#include <string>

struct welcomer : rest::responder<rest::GET> {
  rest::response get(std::string const &path, rest::keywords &data) {
    rest::response response("text/plain");
    response.set_data(std::string("Welcome, ") + data["user"]);
    return response;
  }
};

struct displayer : rest::responder<rest::GET | rest::PUT> {
  rest::response get(std::string const &path, rest::keywords &data) {
    if (path == "list") {
      // return a list of objects
    } else {
      std::string id = data["id"];
      // display the object
    }
  }
  rest::response put(std::string const &path, rest::keywords &data) {
    if (path == "list")
      return 404; // some error code
    else {
      // upload the new object
    }
  }
};

enum mode { COOKIE, KEYWORD };

struct searcher : rest::responder<rest::GET, mode> {
  rest::response get(mode path, rest::keywords &data) {
    rest::response response("text/plain");
    if (path == COOKIE)
      response.set_data("Sorry, got no cookie thing!");
    else
      response.set_data("Am I your searching dork? Hell...");
    return response;
  }
};

int main() {
  rest::context context;
  context.declare_keyword("user", rest::COOKIE);
  welcomer welcome;
  context.bind("/", welcome);
  displayer display;
  context.bind("/list", display);
  context.bind("/object/{id}", display);
  rest::context search;
  searcher search_obj;
  search.bind("/cookie", search_obj, COOKIE);
  search.bind("/keyword/{keyword}", search_obj, KEYWORD);
  context.bind("/search/...", search);
}
