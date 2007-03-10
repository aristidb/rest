// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << argv[0] << ": Need exactly one argument.\n";
    return 1;
  }
  int pipe = open(argv[1], O_RDONLY);
  if (pipe < 0) {
    std::cerr << argv[0] << ": Could not open pipe " << argv[1] << '\n';
    return 2;
  }
  char buf[4096];
  ssize_t n;
  for (;;) {
    n = read(pipe, buf, sizeof(buf));
    if (n < 0) break;
    std::cout.write(buf, n);
  }
}
