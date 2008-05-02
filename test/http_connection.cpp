// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/http_connection.hpp>
#include <rest/host.hpp>
#include <rest/headers.hpp>
#include <rest/logger.hpp>
#include <boost/iostreams/combine.hpp>
#include <sstream>
#include <testsoon.hpp>

namespace {
  rest::network::address ip4(boost::uint32_t x) {
    rest::network::address addr;
    addr.type = rest::network::ip4;
    addr.addr.ip4 = x;
    return addr;
  }
}

TEST_GROUP(no_hosts) {

struct group_fixture_t {
  std::string servername;
  std::stringstream output;
  rest::network::address addr;
  rest::host_container hosts;
  rest::http_connection connection;

  group_fixture_t()
  : servername("SERVERNAME"),
    addr(ip4(0)),
    connection(hosts, addr, servername, new rest::null_logger)
  {
  }

  void serve(std::string const &input) {
    std::istringstream in(input);

    namespace io = boost::iostreams;

    typedef io::combination<std::istringstream, std::stringstream> combination_type;
    combination_type dev = io::combine(boost::ref(in), boost::ref(output));
    std::auto_ptr<std::streambuf> p(new io::stream_buffer<combination_type>(dev));

    connection.serve(p);
  }
};

GFTEST(no request) {
  group_fixture.serve("");
  Equals(group_fixture.output.str(), "");
}

GFTEST(incomplete request) {
  group_fixture.serve("GET incomplete");
  Equals(group_fixture.output.str(), "");
}

GFTEST(bad http version) {
  group_fixture.serve("GET / HHHHHTTP/1.0\r\n");

  std::string first_line;
  std::getline(group_fixture.output, first_line);
  Equals(first_line, "HTTP/1.1 505 HTTP Version Not Supported\r");

  rest::headers headers(*group_fixture.output.rdbuf());
  Equals(headers.get_header("Server", ""), group_fixture.servername);
  Not_equals(headers.get_header("Date", ""), "");
}

}
