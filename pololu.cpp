#include <poll.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>

static void
usage(std::ostream& os, int ret) {
  os << "usage: pololu dev\n";
  os << std::endl;
  exit(ret);
}

static void
HandleJRK(int keyin, int usb) {
  struct pollfd pfds[2] = {
    { .fd = keyin, .events = POLLIN, .revents = 0, },
    { .fd = usb, .events = POLLIN, .revents = 0, },
  };
  const auto nfds = sizeof(pfds) / sizeof(*pfds);
  while(1){
    auto pret = poll(pfds, nfds, -1);
    if(pret < 0){
      std::cerr << "error polling " << nfds << " fds: " << strerror(errno) << std::endl;
      continue;
    }
    for(auto i = 0 ; i < nfds ; ++i){
      if(pfds[i].revents){
        std::cout << "event on " << pfds[i].fd << std::endl; // FIXME
      }
    }
  }
}

int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }
  const char* dev = argv[argc - 1];
  auto fd = open(dev, O_RDWR | O_CLOEXEC | O_NONBLOCK); // FIMXE
  if(fd < 0){
    std::cerr << "couldn't open " << dev << ": " << strerror(errno) << std::endl;
    usage(std::cerr, EXIT_FAILURE);
  }
  HandleJRK(STDIN_FILENO, fd);
  return EXIT_SUCCESS;
}
