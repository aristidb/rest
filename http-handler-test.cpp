// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

#include "http-handler.hpp"

// for Testing purpose
using boost::asio::ip::tcp;
using namespace rest::http;
using namespace rest;

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
                  "<input name=\"Datei\" type=\"file\" size=\"50\""
                  "maxlength=\"100000\" accept=\"text/*\">"
                  "<input type=\"submit\" value=\"Submit\">"
                  "</form></body></html>");

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

int const PORT = 8080;

int main() {
  try {
    boost::asio::io_service io_service;
    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), PORT));
    tester t;
    context c;
    c.bind("/", t);

    for (;;) {
      tcp::iostream stream;
      acceptor.accept(*stream.rdbuf());
      http_handler http(stream);
      response r = http.handle_request(c);
      http.send(r);
    }
  }
  catch(std::exception &e) {
    std::cerr << "ERROR: " << e.what() << "\n";
  }
}
