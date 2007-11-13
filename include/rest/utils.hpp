// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_UTILS
#define REST_UTILS

#include <map>
#include <set>
#include <cerrno>
#include <vector>
#include <iosfwd>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/pipeline.hpp>
#include <boost/iostreams/flush.hpp>
#include <boost/iostreams/write.hpp>
#include <boost/iostreams/read.hpp>
#include <boost/current_function.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>
#include <boost/format.hpp>
#include <boost/ref.hpp>

#include <syslog.h>

namespace rest { namespace utils {


namespace http {
}}}

namespace rest {
  class cookie;
namespace utils { namespace http {
}

}}

#endif
