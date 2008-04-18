// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/object.hpp"
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <map>

using rest::object;
using rest::object_registry;

object::~object() {}


object_registry &object_registry::get() {
  static object_registry obj_reg;
  return obj_reg;
}


class object_registry::impl {
public:
  typedef boost::ptr_vector<object> object_list_type;
  typedef std::pair<std::string, std::string> object_map_key_type;
  typedef std::map<std::pair<std::string, std::string>, object *> object_map_type;

  object_list_type object_list;
  object_map_type object_map;

  static object_map_key_type map_key(
      std::string const &type, std::string const &name, bool to_lower)
  {
    object_map_key_type key(type, name);
    if (to_lower)
      boost::algorithm::to_lower(key.second);
    return key;
  }
};

object_registry::object_registry() : p(new impl) {}
object_registry::~object_registry() {}

void object_registry::add(std::auto_ptr<object> obj_) {
  object *obj = obj_.get();

  p->object_list.push_back(obj_);

  std::string const &type = obj->type();
  p->object_map[impl::map_key(type, obj->name(), false)] = obj;

  object::name_list_type const &aliases = obj->name_aliases();

  for (object::name_list_type::const_iterator it = aliases.begin();
      it != aliases.end();
      ++it)
  {
    p->object_map[impl::map_key(type, *it, false)] = obj;
  }
}

object *object_registry::find(std::string const &type, std::string const &name) const {
  impl::object_map_key_type const &key = impl::map_key(type, name, true);
  return p->object_map[key];
}
