// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/headers.hpp>
#include <rest/utils/http.hpp>
#include <rest/utils/string.hpp>
#include <rest/config.hpp>
#include <boost/none.hpp>
#include <map>

using rest::headers;

class headers::impl {
public:
  typedef std::map<std::string, std::string,
                   rest::utils::string_icompare>
          header_map;

  header_map data;
};

headers::headers() : p(new impl) {
}

headers::headers(std::streambuf &in) : p(new impl) {
  read_headers(in);
}

headers::headers(headers const &o) : p(new impl(*o.p)) {
}

headers::~headers() {
}

void headers::read_headers(std::streambuf &buf) {
  utils::property_tree &tree = config::get().tree();
  std::size_t max_name =
    utils::get(tree, 63, "general", "limits", "max_header_name_length");
  std::size_t max_value =
    utils::get(tree, 1023, "general", "limits", "max_header_value_length");
  std::size_t count =
    utils::get(tree, 64, "general", "limits", "max_header_count");

  utils::http::read_headers(buf, p->data, max_name, max_value, count);
}

boost::optional<std::string> headers::get_header(std::string const &name) const{
  impl::header_map::iterator it = p->data.find(name);
  if (it == p->data.end())
    return boost::none;
  else
    return it->second;
}

std::string headers::get_header(std::string const &n, std::string const &d)const{
  impl::header_map::iterator it = p->data.find(n);
  if (it == p->data.end())
    return d;
  else
    return it->second;
}

void headers::erase_header(std::string const &name) {
  p->data.erase(name);
}

void headers::for_each_header(header_callback const &cb) const {
  for (impl::header_map::iterator it = p->data.begin();
      it != p->data.end();
      ++it)
    cb(it->first, it->second);
}

void headers::set_header(std::string const &name, std::string const &value) {
  p->data[name] = value;
}

void headers::add_header_part(
    std::string const &name, std::string const &value, bool special_asterisk)
{
  impl::header_map::iterator it = p->data.find(name);
  if (it == p->data.end() || it->second.empty()) {
    set_header(name, value);
  } else if (!special_asterisk || it->second != "*") {
    it->second += ", ";
    it->second += value;
  }
}

void headers::write_headers(std::streambuf &out) const {
  for (impl::header_map::const_iterator it = p->data.begin();
      it != p->data.end();
      ++it)
    if (!it->second.empty())
      utils::write_string(out, it->first + ": " + it->second + "\r\n");
}


