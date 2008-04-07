// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/encoding.hpp>
#include <rest/utils/string.hpp>
#include <rest/encodings/identity.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

using rest::encoding;
using rest::encodings_registry;

encoding::~encoding() {}

encodings_registry &encodings_registry::get() {
  static encodings_registry reg;
  return reg;
}

class encodings_registry::impl {
public:
  typedef std::map<
      std::string,
      boost::shared_ptr<encoding>,
      utils::string_icompare
    > map;

  map encodings;
};

encodings_registry::encodings_registry() : p(new impl) {
#undef REST_ENCODING_ADD
#define REST_ENCODING_ADD(e) add_encoding(std::auto_ptr<encoding>(new e))
  using namespace rest::encodings;
  REST_ENCODING_ADD(identity);
}

void encodings_registry::add_encoding(std::auto_ptr<encoding> enc) {
  boost::shared_ptr<encoding> x(enc.release());
  p->encodings.insert(std::make_pair(x->name(), x));

  std::vector<std::string> const &aliases = x->aliases();
  for (std::vector<std::string>::const_iterator it = aliases.begin();
      it != aliases.end();
      ++it)
    p->encodings.insert(std::make_pair(*it, x));
}

encoding *encodings_registry::find_encoding(std::string const &name) const {
  impl::map::const_iterator it = p->encodings.find(name);
  if (it == p->encodings.end())
    return 0;
  return it->second.get();
}
