#include "rest-utils.hpp"
#include <stack>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <stdexcept>
#include <sys/time.h>

namespace rest { namespace utils {
    namespace {
      void handler(int) {
        throw timeout();
      }
    }

    void settimer(long sec, long usec) {
      struct sigaction action;
      action.sa_handler = handler;
      action.sa_flags = SA_RESTART;
      sigemptyset(&action.sa_mask);
      if(::sigaction(SIGALRM, &action, 0x0) == -1)
        throw std::runtime_error("sigaction");

      itimerval timer;
      std::memset(&timer, 0, sizeof(timer));
      timer.it_value.tv_sec = sec;
      timer.it_value.tv_usec = usec;
      if(::setitimer(ITIMER_REAL, &timer, 0x0) == -1)
        throw std::runtime_error("setitimer");
    }

    namespace {
      itimerval old;
    }

    void freeze_timer() {
      itimerval disable;
      memset(&disable, 0, sizeof(disable));
      memset(&old, 0, sizeof(old));
      if(::setitimer(ITIMER_REAL, &disable, &old) == -1)
        throw std::runtime_error("setitimer");
    }

    void unfreeze_timer() {
      if(::setitimer(ITIMER_REAL, &old, 0x0) == -1)
        throw std::runtime_error("setitimer");
    }
}}
