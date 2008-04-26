// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_SIGNALS_HPP
#define REST_SIGNALS_HPP

#include <signal.h>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace rest {

class signals : boost::noncopyable {
public:
  signals();
  ~signals();

  void add(int sig);
  void ignore(int sig);

  void block();

  sigset_t const *member_signals() const;

  sigset_t const *pending_signals() const;
  void reset_pending();

  bool is_pending(int sig) const;

  void handle(int sig);

private:
  class impl;
  boost::scoped_ptr<impl> p;
};

}

#endif
