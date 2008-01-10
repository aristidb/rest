// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/http_connection.hpp"
#include "rest/headers.hpp"
#include "rest/host.hpp"
#include "rest/context.hpp"
#include "rest/input_stream.hpp"
#include "rest/request.hpp"
#include "rest/utils/http.hpp"
#include "rest/utils/log.hpp"
#include "rest/utils/uri.hpp"
#include "rest/utils/chunked_filter.hpp"
#include "rest/utils/length_filter.hpp"
#include "rest/utils/complete_filtering_stream.hpp"
#include "rest/utils/no_flush_writer.hpp"
#include "rest/utils/socket_device.hpp"
#include <rest/config.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/iostreams/combine.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <map>
#include <sstream>
#include <bitset>
#include <memory>
#include <algorithm>

using namespace rest;
namespace det = rest::detail;
namespace algo = boost::algorithm;
namespace io = boost::iostreams;

typedef
  boost::iostreams::stream_buffer<utils::socket_device>
  connection_streambuf;

typedef
  io::combination<
    std::istream,
    std::ostream
  >
  stdio_combination;

typedef io::stream_buffer<stdio_combination> stdio_streambuf;

class http_connection::impl {
public:
  host_container const &hosts;

  std::auto_ptr<std::streambuf> conn;

  std::string const &servername;
  rest::utils::property_tree &tree;

  bool open_;

  enum {
    NO_ENTITY,
    HTTP_1_0_COMPAT,
    X_NO_FLAG
  };
  typedef std::bitset<X_NO_FLAG> state_flags;
  state_flags flags;

  std::set<response::content_encoding_t> encodings;

  request request_;

  typedef std::vector<std::pair<boost::int64_t, boost::int64_t> > ranges_t;
  ranges_t ranges;

  impl(
      host_container const &hosts,
      network::address const &addr,
      std::string const &servername
  )
    : hosts(hosts),
      servername(servername),
      tree(config::get().tree()),
      open_(true),
      request_(addr)
  { }
};

namespace {
  typedef boost::function<
    response (
      http_connection *,
      det::responder_base *,
      keywords &
    )
  > method_handler;

  method_handler H(
      response (http_connection::*fun)(det::responder_base*)
    )
  {
    return boost::bind(fun, _1, _2);
  }

  method_handler H(
      response (http_connection::*fun)(det::responder_base*,keywords&)
    )
  {
    return boost::bind(fun, _1, _2, _3);
  }

  typedef std::map<std::string, method_handler> method_handler_map;

  static std::pair<std::string, method_handler> method_handlers_raw[] = {
    std::make_pair("DELETE", H(&http_connection::handle_delete)),
    std::make_pair("GET", H(&http_connection::handle_get)),
    std::make_pair("HEAD", H(&http_connection::handle_head)),
    std::make_pair("OPTIONS", H(&http_connection::handle_options)),
    std::make_pair("POST", H(&http_connection::handle_post)),
    std::make_pair("PUT", H(&http_connection::handle_put)),
  };

  static method_handler_map const method_handlers(
    method_handlers_raw,
    method_handlers_raw +
      sizeof(method_handlers_raw) / sizeof(*method_handlers_raw)
  );

  std::size_t const get_method_name_length() {
    std::size_t len = 0;
    for (method_handler_map::const_iterator it = method_handlers.begin();
        it != method_handlers.end();
        ++it)
      len = std::max(len, it->first.size());
    return len;
  }

  static std::size_t const method_name_length = get_method_name_length();
}

http_connection::http_connection(
    host_container const &hosts,
    rest::network::address const &addr,
    std::string const &servername)
: p(new impl(hosts, addr, servername))
{}

http_connection::~http_connection() { }

void http_connection::serve(socket_param const &sock, int connfd) {
  p->conn.reset(new connection_streambuf(
        connfd, sock.timeout_read(), sock.timeout_write()));

  serve();
}

void http_connection::serve(std::istream &in, std::ostream &out) {
  p->conn.reset(new stdio_streambuf(
        boost::iostreams::combine(
          boost::ref(in),
          boost::ref(out))));

  serve();
}

void http_connection::reset() {
  p->flags.reset();
  p->encodings.clear();
  impl::ranges_t().swap(p->ranges);
}

void http_connection::send(response r) {
  send(r, !p->flags.test(impl::NO_ENTITY));
}

void http_connection::serve() {
  try {
    while (p->open_) {
      reset();

      response resp(handle_request());

      if (!resp.check_ranges(p->ranges)) {
        // Invalid range - send appropriate response
        boost::int64_t length = resp.length(rest::response::identity);
        response(416).move(resp);
        std::ostringstream range;
        range << "bytes */";
        if (length < 0)
          range << length;
        else
          range << '*';
        resp.get_headers().set_header("Content-Range", range.str());
        impl::ranges_t().swap(p->ranges);
      }

      host const &h = p->request_.get_host();
      if (resp.is_nil())
        h.make_standard_response(resp);
      h.prepare_response(resp);

      p->request_.clear();

      send(resp);
    }
  }
  catch (utils::http::remote_close&) {
  }
  catch (...) {
    p->conn.reset();
    throw;
  }

  p->conn.reset();
}

response http_connection::handle_request() {
  time_t now;
  std::time(&now);

  response out(response::empty_tag());

  std::string method, uri, version;
  std::string host_header;
  std::string etag;
  time_t last_modified = time_t(-1);
  time_t expires = time_t(-1);
  det::responder_base *responder = 0;
  keywords kw;
  det::any_path path_id;

  try {
    boost::tie(method, uri, version) = utils::http::get_request_line(
        *p->conn,
        boost::make_tuple(
          method_name_length,
          utils::get(p->tree, 1023, "general", "max_uri_length"),
          sizeof("HTTP/1.1") - 1 + 5 // 5 additional chars for higher versions
        ));

    utils::log(LOG_INFO, "request: method %s uri %s version %s", method.c_str(),
               uri.c_str(), version.c_str());

    p->request_.set_method(method);

    utils::uri::make_basename(uri);
    p->request_.set_uri(uri);

    if (version == "HTTP/1.0") {
      p->flags.set(impl::HTTP_1_0_COMPAT);
      p->open_ = false;
    } else if (version != "HTTP/1.1") {
      throw 505;
    }

    headers &request_headers = p->request_.get_headers();;

    request_headers.read_headers(*p->conn);

    int ret = set_header_options();
    if (ret != 0)
      return response(ret);

    boost::optional<std::string> host_header_ =
      request_headers.get_header("Host");

    if (!host_header_)
      throw utils::http::bad_format();
    host_header = host_header_.get();

    host const *h = p->hosts.get_host(host_header);
    if (!h)
      throw 404;

    p->request_.set_host(*h);

    context &global = h->get_context();

    context *local;
    global.find_responder(uri, path_id, responder, local, kw);

    if (!responder && !(method == "OPTIONS" && uri == "*"))
      throw 404;

    kw.set_request_data(p->request_);

    if (responder) {
      responder->x_set_path(path_id);
      responder->set_request(p->request_);
      responder->set_keywords(kw);
      responder->set_time(now);
      responder->prepare();

      last_modified = responder->last_modified();
      if (last_modified != time_t(-1) && last_modified > now)
        last_modified = now;
      etag = responder->etag();
      expires = responder->expires();
    }

    int mod_code = handle_modification_tags(
          last_modified == time_t(-1) ? now : last_modified,
          etag,
          method);

    if (!mod_code) {
      method_handler_map::const_iterator m = method_handlers.find(method);
      if (m == method_handlers.end())
        throw 501;
      try {
        m->second(this, responder, kw).move(out);
      } catch (std::exception &e) {
        response(500).move(out);
        out.set_type("text/plain");
        out.set_data(
#ifdef NDEBUG // hide internal information
          "Internal Server Error."
#else
          std::string("Internal error: exception thrown: ") + e.what()
#endif
                     );
      } catch (...) {
        response(500).move(out);
        out.set_type("text/plain");
        out.set_data("Internal error: unknown exception");
      }
    } else {
      response(mod_code).move(out);
    }

  } catch (utils::http::bad_format &) {
    response(400).move(out);
  } catch (int code) {
    response(code).move(out);
  }

  headers &out_headers = out.get_headers();

  out_headers.set_header("Date", utils::http::datetime_string(now));
  tell_allow(out, responder);
  handle_caching(responder, out, now, expires);

  if (last_modified != time_t(-1))
    out_headers.set_header(
        "Last-Modified",
        utils::http::datetime_string(last_modified));
  if (!etag.empty())
    out_headers.set_header("ETag", etag);

  if (method == "GET" || method == "HEAD") {
    int code = out.get_code();
    if (code == -1 || (code >= 200 && code <= 299)) {
      if (out.length(response::identity) >= 0)
        out_headers.set_header("Accept-Ranges", "bytes");
      else
        out_headers.set_header("Accept-Ranges", "none");
    }
  }

  return out;
}

int http_connection::handle_modification_tags(
  time_t last_modified, std::string const &etag, std::string const &method)
{
  int code = 0;
  headers &h = p->request_.get_headers();
  boost::optional<std::string> el;
  el = h.get_header("If-Modified-Since");
  if (el) {
    time_t if_modified_since = utils::http::datetime_value(el.get());
    if (if_modified_since < last_modified)
      return 0;
    code = 304;
  }
  el = h.get_header("If-Unmodified-Since");
  if (el) {
    time_t if_unmodified_since = utils::http::datetime_value(el.get());
    if (if_unmodified_since >= last_modified)
      return 0;
    code = 412;
  }
  el = h.get_header("If-Match");
  if (el) {
    if (el.get() == "*") {
      if (!etag.empty())
        return 0;
    } else {
      std::vector<std::string> if_match;
      utils::http::parse_list(el.get(), if_match);
      if (std::find(if_match.begin(), if_match.end(), etag) != if_match.end())
        return 0;
    }
    code = 412;
  }
  el = h.get_header("If-None-Match");
  if (el) {
    if (el.get() == "*") {
      if (etag.empty())
        return 0;
    } else {
      std::vector<std::string> if_none_match;
      utils::http::parse_list(el.get(), if_none_match);
      if (std::find(if_none_match.begin(), if_none_match.end(), etag) ==
          if_none_match.end())
        return 0;
    }
    if (code != 412)
      code = (method == "GET" || method == "HEAD") ? 304 : 412;
  }
  el = h.get_header("If-Range");
  if (el) {
    time_t v = utils::http::datetime_value(el.get());
    if (v == time_t(-1)) {
      if (el.get() != etag)
        h.erase_header("Range");
    } else {
      if (v <= last_modified)
        h.erase_header("Range");
    }
  }
  return code;
}

void http_connection::handle_caching(
  det::responder_base *responder,
  response &resp,
  time_t now,
  time_t expires)
{
  headers &out_headers = resp.get_headers();

  bool cripple_expires = false;
  cache::flags general = cache::private_ | cache::no_cache | cache::no_store;
  if (responder && !never_cache(resp.get_code()))
    general = responder->cache();

  if (general & cache::private_) {
    cripple_expires = true;
    out_headers.add_header_part("Cache-Control", "private");
  } else {
    out_headers.add_header_part("Cache-Control", "public");
  }

  if (general & cache::no_cache) {
    out_headers.add_header_part("Cache-Control", "no-cache");
    out_headers.add_header_part("Pragma", "no-cache");
  }

  if (general & cache::no_store)
    out_headers.add_header_part("Cache-Control", "no-store");

  if (general & cache::no_transform)
    out_headers.add_header_part("Cache-Control", "no-transform");

  if (responder) {
    out_headers.for_each_header(boost::bind(
        &http_connection::handle_header_caching,
        this,
        responder,
        boost::ref(resp),
        boost::ref(cripple_expires),
        _1));
  }

  if (expires != time_t(-1)) {
    std::ostringstream s_max_age;
    time_t max_age = (expires > now) ? (expires - now) : 0;
    s_max_age << "max-age=" << max_age;
    out_headers.add_header_part("Cache-Control", s_max_age.str());

    if (!cripple_expires)
      out_headers.set_header("Expires", utils::http::datetime_string(expires));
  } else if (!cripple_expires) {
    out_headers.set_header("Expires", "");
  }

  if (cripple_expires)
    out_headers.set_header("Expires", "0");
}

bool http_connection::never_cache(int code) {
  switch (code) {
  case -1:  // ---
  case 200: // OK
  case 203: // Non-Authoritative Information
  case 206: // Partial content
    return false;
  case 300: // Multiple Choices
  case 301: // Moved Permanently
    return false;
  case 410: // Gone
    return false;
  default:
    return true;
  };
}

void http_connection::handle_header_caching(
  det::responder_base *responder,
  response &resp,
  bool &cripple_expires,
  std::string const &header)
{
  cache::flags flags = responder->cache(header);
  if (flags & cache::no_cache) {
    cripple_expires = true;
    std::string x;
    x += "no-cache="; x += '"'; x += header; x += '"';
    resp.get_headers().add_header_part("Cache-Control",  x);
  }
  if (flags & cache::private_) {
    cripple_expires = true;
    std::string x;
    x += "private="; x += '"'; x += header; x += '"';
    resp.get_headers().add_header_part("Cache-Control",  x);
  }
}

response http_connection::handle_options(det::responder_base *) {
  response resp(200);
  return resp;
}

response http_connection::handle_get(det::responder_base *responder) {
  det::get_base *getter = responder->x_getter();
  if (!getter || !responder->exists())
    return response(404);

  response r(getter->get());

  int code = r.get_code();
  if ((code == -1 || (code >= 200 && code <= 299)) && !r.is_nil())
    analyze_ranges();

  return r;
}

response http_connection::handle_head(det::responder_base *responder) {
  //TODO: better implementation
  p->flags.set(impl::NO_ENTITY);
  det::get_base *getter = responder->x_getter();
  if (!getter || !responder->exists())
    return response(404);

  return getter->get();
}


response http_connection::handle_post(
    det::responder_base *responder, keywords &kw)
{
  det::post_base *poster = responder->x_poster();
  if (!poster || !responder->exists())
    return response(404);

  int ret = handle_entity(kw);
  if (ret != 0)
    return response(ret);

  return poster->post();
}

response http_connection::handle_put(
    det::responder_base *responder, keywords &kw)
{
  det::put_base *putter = responder->x_putter();
  if (!putter)
    return response(404);

  int ret = handle_entity(kw);
  if(ret != 0)
    return response(ret);

  return putter->put();
}

response http_connection::handle_delete(det::responder_base *responder) {
  det::delete__base *deleter = responder->x_deleter();
  if (!deleter || !responder->exists())
    return response(404);
  return deleter->delete_();
}

void http_connection::tell_allow(response &resp, det::responder_base *responder)
{
  headers &h = resp.get_headers();
  h.add_header_part("Allow", "OPTIONS");
  if (!responder) {
    h.add_header_part("Allow", "DELETE");
    h.add_header_part("Allow", "GET");
    h.add_header_part("Allow", "HEAD");
    h.add_header_part("Allow", "POST");
    h.add_header_part("Allow", "PUT");
  } else {
    if (responder->x_getter()) {
      h.add_header_part("Allow", "GET");
      h.add_header_part("Allow", "HEAD");
    }
    if (responder->x_poster()) {
      h.add_header_part("Allow", "POST");
    }
    if (responder->x_deleter()) {
      h.add_header_part("Allow", "DELETE");
    }
    if (responder->x_putter()) {
      h.add_header_part("Allow", "PUT");
    }
  }
}

int http_connection::set_header_options() {
  headers &h = p->request_.get_headers();
  boost::optional<std::string> connect_header = h.get_header("Connection");
  if (connect_header) {
    std::vector<std::string> tokens;
    utils::http::parse_list(connect_header.get(), tokens);
    for (std::vector<std::string>::iterator it = tokens.begin();
        it != tokens.end();
        ++it) {
      algo::to_lower(*it);
      if (*it == "close")
        p->open_ = false;
      if (p->flags.test(impl::HTTP_1_0_COMPAT))
        h.erase_header(*it);
    }
  }

  typedef std::multimap<int, std::string> qlist_t;

  qlist_t qlist;
  boost::optional<std::string> accept_encoding= h.get_header("Accept-Encoding");
  if (accept_encoding)
    utils::http::parse_qlist(accept_encoding.get(), qlist);

  qlist_t::const_reverse_iterator const rend = qlist.rend();
  bool found = false;
  for(qlist_t::const_reverse_iterator i = qlist.rbegin(); i != rend; ++i) {
    if(i->first == 0) {
      if(!found && (i->second == "identity" || i->second == "*"))
        return 406;
    }
    else {
      if(i->second == "gzip" || i->second == "x-gzip") {
        p->encodings.insert(response::gzip);
        found = true;
      }
      else if(i->second == "bzip2" || i->second == "x-bzip2") {
        p->encodings.insert(response::bzip2);
        found = true;
      }
      else if(i->second == "deflate") {
        p->encodings.insert(response::deflate);
        found = true;
      }
      else if(i->second == "identity")
        found = true;
    }
  }

  return 0;
}

void http_connection::analyze_ranges() {
  boost::optional<std::string> range_ =
    p->request_.get_headers().get_header("Range");

  if (!range_)
    return;
  std::string const &range = range_.get();

  typedef std::vector<std::string> s_vect;
  s_vect seq;

  rest::utils::http::parse_list(range, seq, '=');
  if (seq.size() != 2 || !algo::iequals(seq[0], "bytes"))
    return;

  std::string data;
  seq[1].swap(data);
  seq.clear();

  rest::utils::http::parse_list(data, seq, ',');
  if (seq.empty())
    return;

  for (s_vect::iterator it = seq.begin(); it != seq.end(); ++it) {
    std::vector<std::string> x;
    rest::utils::http::parse_list(*it, x, '-', false);
    if (x.size() != 2)
      goto bad;
    std::pair<boost::int64_t, boost::int64_t> v(-1, -1);
    try {
      if (!x[0].empty()) {
        v.first = boost::lexical_cast<long>(x[0]);
        if (v.first < 0)
          goto bad;
      }
      if (!x[1].empty()) {
        v.second = boost::lexical_cast<long>(x[1]);
        if (v.second < 0)
          goto bad;
      }
    } catch (boost::bad_lexical_cast &) {
      goto bad;
    }
    if (v.first == -1 && v.second == -1)
      goto bad;
    if (v.first != -1 && v.second != -1 && v.first > v.second)
      goto bad;

    p->ranges.push_back(v);
  }

  return;

  bad:
    impl::ranges_t().swap(p->ranges);
    return;
}

int http_connection::handle_entity(keywords &kw) {
  std::auto_ptr<io::filtering_streambuf<io::input> > fin
    (new io::filtering_streambuf<io::input>);

  headers &h = p->request_.get_headers();

  boost::optional<std::string> content_encoding =
    h.get_header("Content-Encoding");

  if (content_encoding) {
    std::vector<std::string> ce;
    utils::http::parse_list(content_encoding.get(), ce);
    for (std::vector<std::string>::iterator it = ce.begin();
        it != ce.end();
        ++it)
    {
      if (algo::iequals(content_encoding.get(), "gzip") ||
           (p->flags.test(impl::HTTP_1_0_COMPAT) &&
             algo::iequals(content_encoding.get(), "x-gzip")))
      {
        fin->push(io::gzip_decompressor());
      } else if (algo::iequals(content_encoding.get(), "bzip2")) {
        fin->push(io::bzip2_decompressor());
      } else if (algo::iequals(content_encoding.get(), "deflate")) {
        io::zlib_params z;
        z.noheader = true;
        fin->push(io::zlib_decompressor(z));
      } else if (!algo::iequals(content_encoding.get(), "identity")) {
        return 415;
      }
    }
  }

  boost::optional<std::string> transfer_encoding =
    h.get_header("Transfer-Encoding");

  bool chunked = false;

  if (transfer_encoding) {
    std::vector<std::string> te;
    utils::http::parse_list(transfer_encoding.get(), te);
    for (std::vector<std::string>::iterator it = te.begin();
        it != te.end();
        ++it)
    {
      if(algo::iequals(*it, "chunked")) {
        if (it != --te.end())
          return 400;
        chunked = true;
        fin->push(utils::chunked_filter());
      } else if (algo::iequals(*it, "gzip")) {
        fin->push(io::gzip_decompressor());
      } else if (algo::iequals(*it, "deflate")) {
        fin->push(io::zlib_decompressor());
      } else if (!algo::iequals(*it, "identity")) {
        return 501;
      }
    }
  }

  boost::optional<std::string> content_length =
    h.get_header("Content-Length");

  if (!content_length && !chunked) {
    return 411; // Content-length required
  } else if (!chunked) {
    boost::uint64_t const length =
      boost::lexical_cast<boost::uint64_t>(content_length.get());

    fin->push(utils::length_filter(length));
  }

  if (!p->flags.test(impl::HTTP_1_0_COMPAT)) {
    std::string expect = h.get_header("Expect", "");
    if (!expect.empty()) {
      if (!algo::iequals(expect, "100-continue"))
        return 417;
      send(response(100), false);
    }
  }

  fin->push(boost::ref(*p->conn), 0, 0);

  std::string content_type =
    h.get_header("Content-Type", "application/octet-stream");

  input_stream pstream(new utils::complete_filtering_stream(fin.release()));
  kw.set_entity(pstream, content_type);

  return 0;
}

void http_connection::send(response r, bool entity) {
  headers &h = r.get_headers();

  io::stream<utils::no_flush_writer> out(p->conn.get());

  if (p->flags.test(impl::HTTP_1_0_COMPAT))
    out << "HTTP/1.0 ";
  else
    out << "HTTP/1.1 ";

  int code = r.get_code();
  if (code == -1)
    code = 200;

  utils::log(LOG_NOTICE, "response: %d", code);

  out << code << " " << response::reason(code) << "\r\n";

  if (code >= 400)
    p->open_ = false;

  if (!p->flags.test(impl::HTTP_1_0_COMPAT) && !p->open_ && code != 100)
    h.add_header_part("Connection", "close", false);

  h.set_header("Server", p->servername);

  if (entity) {
    bool may_chunk = !p->flags.test(impl::HTTP_1_0_COMPAT);

    response::content_encoding_t enc =
      r.choose_content_encoding(p->encodings, !p->ranges.empty());

    switch (enc) {
    case response::gzip:
      h.set_header("Content-Encoding", "gzip");
      break;
    case response::bzip2:
      h.set_header("Content-Encoding", "bzip2");
      break;
    case response::deflate:
      h.set_header("Content-Encoding", "deflate");
      break;
    default: break;
    }

    if (p->ranges.empty() && !r.chunked(enc))
      h.set_header("Content-Length", r.length(enc));
    else if (may_chunk)
      h.set_header("Transfer-Encoding", "chunked");

    r.print_headers(out);
    r.print_entity(*out.rdbuf(), enc, may_chunk, p->ranges);
  } else {
    r.print_headers(out);
  }

  io::flush(out);
  out->real_flush();
}
// Local Variables: **
// mode: C++ **
// coding: utf-8 **
// c-electric-flag: nil **
// End: **
