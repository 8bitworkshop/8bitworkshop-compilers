// sdcc is built with -fno-exceptions (see Makefile): Boost.Graph calls this
// instead of throwing, and a failure is fatal for the compiler anyway.
#include <boost/throw_exception.hpp>
#include <cstdio>
#include <cstdlib>

namespace boost {
void throw_exception(std::exception const &e, boost::source_location const &) {
  fprintf(stderr, "sdcc: fatal: %s\n", e.what());
  abort();
}
}
