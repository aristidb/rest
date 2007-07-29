// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

#include "rest.hpp"
#include "rest-config.hpp"
#include <boost/lambda/lambda.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>

struct tester : rest::responder<rest::GET | rest::PUT | rest::DELETE |
                                rest::POST> {
  rest::response get(std::string const &path, rest::keywords &) {
    std::cout << "GET: " << path << '\n';

    rest::response resp("text/html");
    #if 0
    resp.set_data("<html><head><title>supi</title></head>\n"
                  "<body>\n<h3>Allles Supi!!</h3>\n<blink>blink</blink>\n"
                  "<form name=\"input\" action=\"/\" "
                  "method=\"post\" "
                  "enctype=\"multipart/form-data\" "
                  ">\n"
                  "<input type=\"text\" name=\"user\">\n"
                  "<input type=\"text\" name=\"bar\">\n"
                  "<input type=\"text\" name=\"bar\">\n"
                  "<input name=\"datei\" type=\"file\" size=\"50\" "
                  "maxlength=\"100000\" accept=\"text/*\">\n"
                  "<input type=\"submit\" value=\"Submit\">\n"
                  "</form>\n</body>\n</html>\n");
    #endif
    resp.set_data(
      rest::input_stream(new std::ifstream("out.html.gz")),
      true,
      rest::response::gzip);

    return resp;
  }
  rest::response put(std::string const &path, rest::keywords &) {
    std::cout << "PUT: " << path << '\n';
    rest::response ok(200);
    return ok;
  }
  rest::response delete_(std::string const &path, rest::keywords &) {
    std::cout << "DELETE: " << path << '\n';
    return rest::response(200);
  }
  rest::response post(std::string const &path, rest::keywords &kw) {
    std::cout << "POST: " << path << '\n';

    rest::response resp("text/plain");

    std::ostringstream out;

    for (int i = 0; kw.exists("bar", i); ++i) {
      std::string bar = kw.access("bar", i);
      out << "bar: " << bar << std::endl;
      std::cout << "bar: " << bar << std::endl;
    }

    std::string fln;
    std::getline(kw.read("datei"), fln);
    out << "file: " << fln << '\n';
    std::cout << "file: " << fln << std::endl;

    std::string user = kw["user"];
    out << "user: " << user << '\n';
    std::cout << "user: " << user << std::endl;

    std::string file = kw["datei"];
    out << "file (oh we read() it): " << file << std::endl;
    std::cout << "file (oh we read() it): " << file << std::endl;

    resp.set_data(out.str());
    return resp;
  }
};

using namespace boost::lambda;

int main(int argc, char **argv) {
  std::cout << "STARTING, pid: " << getpid() << std::endl;

  tester t;

  rest::host h("");
  h.get_context().bind("/", t);
  h.get_context().declare_keyword("user", rest::FORM_PARAMETER);
  h.get_context().declare_keyword("bar", rest::FORM_PARAMETER);
  h.get_context().declare_keyword("datei", rest::FORM_PARAMETER);

  rest::server s(*rest::config(argc, argv));
  std::for_each(s.sockets_begin(), s.sockets_end(), rest::host::add(h));

  s.serve();
}
