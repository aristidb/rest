// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

#include "rest.hpp"
#include "rest-config.hpp"
#include <iostream>
#include <sys/types.h>
#include <unistd.h>

struct tester : rest::responder<rest::GET | rest::PUT | rest::DELETE |
                                rest::POST> {
  rest::response get(std::string const &path, rest::keywords &) {
    std::cout << "GET: " << path << '\n';

    rest::response resp("text/html");
    resp.set_data("<html><head><title>supi</title></head>"
                  "<body><h3>Allles Supi!!</h3><blink>blink</blink>"
                  "<form name=\"input\" action=\"/\""
                  "method=\"post\" enctype=\"multipart/form-data\">"
                  "<input type=\"text\" name=\"user\">"
                  "<input type=\"text\" name=\"bar\">"
                  "<input type=\"text\" name=\"bar\">"
                  "<input name=\"Datei\" type=\"file\" size=\"50\""
                  "maxlength=\"100000\" accept=\"text/*\">"
                  "<input type=\"submit\" value=\"Submit\">"
                  "</form></body></html>\n");

    return resp;
  }
  rest::response put(std::string const &path, rest::keywords &) {
    std::cout << "PUT: " << path << '\n';
    return 200;
  }
  rest::response delete_(std::string const &path, rest::keywords &) {
    std::cout << "DELETE: " << path << '\n';
    return 200;
  }
  rest::response post(std::string const &path, rest::keywords &) {
    std::cout << "POST: " << path << '\n';
    return 200;
  }
};

int main(int argc, char **argv) {
  std::cout << "STARTING, pid: " << getpid() << std::endl;
  try {
    rest::host h("");
    tester t;
    h.get_context().bind("/", t);
    rest::server s(*rest::config(argc, argv));
    s.add_socket(rest::server::socket_param
                 (8080, rest::server::socket_param::ip4))->add_host(h);
    s.serve();
  } catch(rest::utils::end &e) {
  }
}
