#include <poll.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>

static void
usage(std::ostream& os, int ret) {
  os << "usage: pololu dev\n";
  os << std::endl;
  exit(ret);
}

static void
KeyboardHelp() {
  std::cout << "(i) read input (t) read target (f) read feedback (h) help" << std::endl;
}

static void
WriteJRKCommand(int cmd, int fd) {
  assert(cmd >=0);
  assert(cmd < 0x100); // commands are a single byte
  unsigned char cmdbuf[1] = { (unsigned char)(cmd % 0x100u) };
  auto ss = ::write(fd, cmdbuf, sizeof(cmdbuf));
  if(ss < 0 || (size_t)ss < sizeof(cmdbuf)){
    std::cerr << "error writing command to " << fd << ": " << strerror(errno) << std::endl;
    // FIXME throw exception? hrmmmm
  }
  std::cout << "wrote to " << fd << ": " << ss << std::endl;
}

static void
ReadJRKInput(int fd) {
  WriteJRKCommand(0xa1, fd);
}

static void
ReadJRKFeedback(int fd) {
  WriteJRKCommand(0xa3, fd);
}

static void
ReadJRKTarget(int fd) {
  WriteJRKCommand(0xa5, fd);
}

static void
HandleKeypress(int fd, int usbfd) {
  char buf[1];
  while((read(fd, buf, sizeof(buf))) == 1){
    switch(buf[0]){
      case 'h': KeyboardHelp(); break;
      case 'i': ReadJRKInput(usbfd); break;
      case 't': ReadJRKTarget(usbfd); break;
      case 'f': ReadJRKFeedback(usbfd); break;
      default:
        break;
    }
  }
  if(errno != EAGAIN){
    std::cerr << "error reading keypress: " << strerror(errno) << std::endl;
    // FIXME throw exception?
  }
}

static void
HandleUSB(int fd) {
  std::cout << "reading from " << fd << std::endl; // FIXME
}

static void
HandleJRK(int keyin, int usb) {
  struct pollfd pfds[2] = {
    { .fd = keyin, .events = POLLIN, .revents = 0, },
    { .fd = usb, .events = POLLIN, .revents = 0, },
  };
  const auto nfds = sizeof(pfds) / sizeof(*pfds);
  while(1){
    // FIXME catch fatal signals so terminal can be restored
    auto pret = poll(pfds, nfds, -1);
    if(pret < 0){
      std::cerr << "error polling " << nfds << " fds: " << strerror(errno) << std::endl;
      continue;
    }
    for(auto i = 0u ; i < nfds ; ++i){
      if(pfds[i].revents){
        if(pfds[i].fd == keyin){
          HandleKeypress(keyin, usb);
        }else if(pfds[i].fd == usb){
          HandleUSB(usb);
        }else{
          std::cout << "event on bogon fd " << pfds[i].fd << std::endl; // FIXME
        }
      }
    }
  }
}

int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }
  const char* dev = argv[argc - 1];
  auto fd = open(dev, O_RDWR | O_CLOEXEC | O_NONBLOCK); // FIMXE need we be nonblocking?
  if(fd < 0){
    std::cerr << "couldn't open " << dev << ": " << strerror(errno) << std::endl;
    usage(std::cerr, EXIT_FAILURE);
  }
  auto infd = STDIN_FILENO;
  struct termios oldterm;
  if(tcgetattr(infd, &oldterm)){
    std::cerr << "couldn't save terminal settings on " << infd << ": "
      << strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }
  auto keyterm = oldterm;
  keyterm.c_lflag &= ~(ICANON | ECHO);
  if(tcsetattr(infd, TCSANOW, &keyterm)){
    std::cerr << "couldn't set terminal settings on " << infd << ": "
      << strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }
  std::cout << "Opened Pololu jrk on fd " << fd << " at " << dev << std::endl;
  KeyboardHelp();
  HandleJRK(infd, fd);
  if(tcsetattr(infd, TCSANOW, &oldterm)){
    std::cerr << "couldn't restore terminal settings on " << infd << ": "
      << strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
