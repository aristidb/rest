// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

#include "rest.hpp"
#include "rest-config.hpp"
#include <boost/lambda/lambda.hpp>
#include <iostream>
#include <algorithm>
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
                  "method=\"post\" "
                  //"enctype=\"multipart/form-data\""
                  ">"
                  "<input type=\"text\" name=\"user\">"
                  "<input type=\"text\" name=\"bar\">"
                  "<input type=\"text\" name=\"bar\">"
                  "<input name=\"datei\" type=\"file\" size=\"50\""
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
  try {
    rest::host h("");
    tester t;
    h.get_context().bind("/", t);
    h.get_context().declare_keyword("user", rest::FORM_PARAMETER);
    h.get_context().declare_keyword("bar", rest::FORM_PARAMETER);
    h.get_context().declare_keyword("datei", rest::FORM_PARAMETER);
    rest::server s(*rest::config(argc, argv));
    for(rest::server::sockets_iterator i = s.sockets_begin();
        i != s.sockets_end();
        ++i)
      i->add_host(h);

    s.serve();
  } catch(rest::utils::end &e) {
  }
}
