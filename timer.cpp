#include "rest-util.hpp"

namespace rest { namespace util {
typedef void (*timeout)(int);
void killer(int);
void settimer(long sec, long usec, timeout t);
void freeze_timer();
void unfreeze_timer();
}}
