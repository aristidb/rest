// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/server.hpp"
#include "rest/host.hpp"
#include "rest/context.hpp"
#include "rest/cookie.hpp"
#include "rest/config.hpp"
#include "rest/utils/log.hpp"
#include <boost/lambda/lambda.hpp>
#include <fstream>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>

struct tester : rest::responder<rest::ALL, rest::NO_PATH> {
  std::string etag() const {
    return "\"zxyl\"";
  }

  time_t last_modified(time_t now) const {
    return now;
  }

  time_t expires(time_t now) const {
    return now + 10;
  }

  rest::cache::flags cache() const {
    return rest::cache::private_ | rest::cache::no_transform;
  }

  rest::response get(rest::keywords &kw, rest::request const &) {
    rest::response resp("text/html");
    resp.add_cookie(rest::cookie("hello", "world"));
    resp.add_cookie(rest::cookie("foo", "bar"));
    #if 1
    std::string r("<html><head><title>supi</title></head>\n"
                  "<body>\n<h3>Allles Supi!!</h3>\n<blink>blink</blink>\n"
                  "<form name=\"input\" action=\"/\" "
                  "method=\"post\" "
                  "enctype=\"multipart/form-data\" "
                  "accept-charset=\"ISO-8859-1\" "
                  ">\n"
                  "<input type=\"text\" name=\"user\">\n"
                  "<input type=\"text\" name=\"bar\">\n"
                  "<input type=\"text\" name=\"bar\">\n"
                  "<input name=\"datei\" type=\"file\" size=\"50\" "
                  "maxlength=\"100000\" accept=\"text/*\">\n"
                  "<input type=\"submit\" value=\"Submit\">\n"
                  "</form>\n<p>User Agent: &quot;");
    r += kw["user-agent"];
    r += "&quot;</p>\n</body>\n</html>\n";
    resp.set_data(r);
    #else
    resp.set_data(
      rest::input_stream(new std::ifstream("out.html.gz")),
      true,
      rest::response::gzip);
    #endif

    return resp;
  }
  rest::response put(rest::keywords &, rest::request const &) {
    rest::response ok(200);
    return ok;
  }
  rest::response delete_(rest::keywords &, rest::request const &) {
    return rest::response(200);
  }
  rest::response post(rest::keywords &kw, rest::request const &) {
    rest::response resp("text/plain");

    std::ostringstream out;

    for (int i = 0; kw.exists("bar", i); ++i) {
      std::string bar = kw.access("bar", i);
      out << "bar: " << bar << std::endl;
    }

    std::string fln;
    std::getline(kw.read("datei"), fln);
    out << "file: " << fln << '\n';

    out << "user: " << kw["user"] << '\n';

    out << "file (oh we read() it): " << kw["datei"] << '\n';

    out << "user agent: " << kw["user-agent"] << std::endl;

    out << "cookie: " << kw["cookie"] << std::endl;

    out << "the_cookie: " << kw["the_cookie"] << std::endl;
    resp.add_cookie(rest::cookie("the_cookie", kw["the_cookie"] + "+"));

    out << "hello: " << kw["hello"] << std::endl;

    resp.set_data(out.str());
    return resp;
  }
};

using namespace boost::lambda;

int main(int argc, char **argv) {
  try {
    tester t;

    rest::host h("");
    rest::context &c = h.get_context();

    c.bind("/", t);
    c.declare_keyword("user", rest::FORM_PARAMETER);
    c.declare_keyword("bar", rest::FORM_PARAMETER);
    c.declare_keyword("datei", rest::FORM_PARAMETER);
    c.declare_keyword("user-agent", rest::HEADER);
    c.declare_keyword("cookie", rest::HEADER);
    c.declare_keyword("the_cookie", rest::COOKIE);
    c.declare_keyword("hello", rest::COOKIE);

    rest::config &conf = rest::config::get();
    rest::utils::property_tree &tree = conf.tree();

    rest::utils::set(tree, "musikdings.rest/0.1", "general", "name");
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
