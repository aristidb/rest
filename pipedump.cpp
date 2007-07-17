// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <cerrno>
#include <cstring>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <fifo>\n";
    return 1;
  }
start:
  int pipe = open(argv[1], O_RDONLY);
  if (pipe < 0) {
    std::cerr << argv[0] << ": Could not open pipe " << argv[1]
              << ": `" << strerror(errno) << "'\n";
    return 2;
  }

  char buf[4096];
  ssize_t n;
  for (;;) {
    n = read(pipe, buf, sizeof(buf));
    if (n < 0)
      break;
    else if (n == 0) {
      close(pipe);
      goto start;
    }
    std::cout.write(buf, n);
    std::cout.flush();
  }
}
