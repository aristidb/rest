// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:

#include "rest.hpp"
#include "rest-config.hpp"
#include "rest-utils.hpp"
#include <boost/lambda/lambda.hpp>
#include <fstream>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>

struct tester : rest::responder<rest::GET | rest::PUT | rest::DELETE |
                                rest::POST> {
  std::string etag(std::string const &path) const {
    if (path == "/")
      return "\"zxyl\"";
    return "";
  }

  time_t last_modified(std::string const &, time_t now) const {
    return now;
  }

  rest::response get(std::string const &, rest::keywords &) {
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
  rest::response put(std::string const &, rest::keywords &) {
    rest::response ok(200);
    return ok;
  }
  rest::response delete_(std::string const &, rest::keywords &) {
    return rest::response(200);
  }
  rest::response post(std::string const &, rest::keywords &kw) {
    rest::response resp("text/plain");

    std::ostringstream out;

    for (int i = 0; kw.exists("bar", i); ++i) {
      std::string bar = kw.access("bar", i);
      out << "bar: " << bar << std::endl;
    }

    std::string fln;
    std::getline(kw.read("datei"), fln);
    out << "file: " << fln << '\n';

    std::string user = kw["user"];
    out << "user: " << user << '\n';

    std::string file = kw["datei"];
    out << "file (oh we read() it): " << file << std::endl;

    resp.set_data(out.str());
    return resp;
  }
};

using namespace boost::lambda;

int main(int argc, char **argv) {
  try {
    tester t;

    rest::host h("");
    h.get_context().bind("/", t);
    h.get_context().declare_keyword("user", rest::FORM_PARAMETER);
    h.get_context().declare_keyword("bar", rest::FORM_PARAMETER);
    h.get_context().declare_keyword("datei", rest::FORM_PARAMETER);

    rest::config &conf = rest::config::get();
    rest::utils::property_tree &tree = conf.tree();

    rest::utils::set(tree, "musikdings.rest/1.1", "general", "name");
    conf.load(argc, argv);

    rest::server s(tree);
    std::for_each(s.sockets_begin(), s.sockets_end(), rest::host::add(h));

    s.serve();
    return 0;
  } catch (std::exception &e) {
    rest::utils::log(LOG_CRIT, "unexpected critical exception: %s", e.what());
    return 1;
  } catch (...) {
    rest::utils::log(LOG_CRIT, "unexpected critical exception");
    return 1;
  }
}
