#include <queue>
#include <poll.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>

constexpr unsigned char JRKCMD_READ_INPUT = 0xa1;
constexpr unsigned char JRKCMD_READ_FEEDBACK = 0xa3;
constexpr unsigned char JRKCMD_READ_TARGET = 0xa5;
constexpr unsigned char JRKCMD_READ_ERRORS = 0xb5;

static void
usage(std::ostream& os, int ret) {
  os << "usage: pololu dev\n";
  os << std::endl;
  exit(ret);
}

std::queue<unsigned char> sent_cmds;

static void
KeyboardHelp() {
  std::cout << "(i) read input (t) read target (f) read feedback (e) read errors" << "\n";
  std::cout << "(h) print help" << std::endl;
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
}

static void
SendJRKReadCommand(int cmd, std::queue<unsigned char>& cmdq, int fd) {
  WriteJRKCommand(cmd, fd);
  cmdq.push(cmd);
}

static void
ReadJRKInput(int fd) {
  constexpr unsigned char cmd = JRKCMD_READ_INPUT;
  SendJRKReadCommand(cmd, sent_cmds, fd);
}

static void
ReadJRKFeedback(int fd) {
  constexpr auto cmd = JRKCMD_READ_FEEDBACK;
  SendJRKReadCommand(cmd, sent_cmds, fd);
}

static void
ReadJRKTarget(int fd) {
  constexpr auto cmd = JRKCMD_READ_TARGET;
  SendJRKReadCommand(cmd, sent_cmds, fd);
}

static void
ReadJRKErrors(int fd) {
  constexpr auto cmd = JRKCMD_READ_ERRORS;
  SendJRKReadCommand(cmd, sent_cmds, fd);
}

static void
HandleKeypress(int fd, int usbfd) {
  char buf[1];
  errno = 0;
  while((read(fd, buf, sizeof(buf))) == 1){
    switch(buf[0]){
      case 'h': KeyboardHelp(); break;
      case 'i': ReadJRKInput(usbfd); break;
      case 't': ReadJRKTarget(usbfd); break;
      case 'f': ReadJRKFeedback(usbfd); break;
      case 'e': ReadJRKErrors(usbfd); break;
      default:
        break;
    }
  }
  if(errno != EAGAIN){
    std::cerr << "error reading keypress: " << strerror(errno) << std::endl;
    // FIXME throw exception?
  }
}

static inline
std::ostream& HexOutput(std::ostream& s, const unsigned char* data, size_t len) {
  std::ios state(NULL);
  state.copyfmt(s);
  s << std::hex;
  for(size_t i = 0 ; i < len ; ++i){
    s << std::setfill('0') << std::setw(2) << (int)data[i];
  }
  s.copyfmt(state);
  return s;
}

static void
HandleUSB(int fd) {
  constexpr auto bufsize = 2;
  unsigned char valbuf[bufsize];
  errno = 0;

  while((read(fd, valbuf, bufsize)) == bufsize){
    int sword = valbuf[1] * 256 + valbuf[0];
    std::cout << "received bytes: 0x";
    HexOutput(std::cout, valbuf, sizeof(valbuf)) << " (" << sword << ")" << std::endl;
    if(sent_cmds.empty()){
      std::cerr << "warning: no outstanding command for recv" << std::endl;
      continue;
    }
    unsigned char expcmd = sent_cmds.front();
    sent_cmds.pop();
    switch(expcmd){
      case JRKCMD_READ_INPUT: std::cout << "Input is " << sword; break;
      case JRKCMD_READ_FEEDBACK: std::cout << "Feedback is " << sword; break;
      case JRKCMD_READ_TARGET: std::cout << "Target is " << sword; break;
      case JRKCMD_READ_ERRORS:
        std::cout << "Error bits: " <<
          ((sword & 0x0001) ? "AwaitingCmd" : "") <<
          ((sword & 0x0002) ? "NoPower" : "") <<
          ((sword & 0x0004) ? "DriveError" : "") <<
          ((sword & 0x0008) ? "InvalidInput" : "") <<
          ((sword & 0x0010) ? "InputDisconn" : "") <<
          ((sword & 0x0020) ? "FdbckDisconn" : "") <<
          ((sword & 0x0040) ? "AmpsExceeded" : "") <<
          ((sword & 0x0080) ? "SerialSig" : "") <<
          ((sword & 0x0100) ? "UARTOflow" : "") <<
          ((sword & 0x0200) ? "SerialOflow" : "") <<
          ((sword & 0x0400) ? "SerialCRC" : "") <<
          ((sword & 0x0800) ? "SerialProto" : "") <<
          ((sword & 0x1000) ? "TimeoutRX" : "") <<
          std::endl;
        break;
      default:
        std::cerr << "unexpected command " << (int)expcmd;
    }
  }
  if(errno != EAGAIN){
    std::cerr << "error reading serial: " << strerror(errno) << std::endl;
    // FIXME throw exception?
  }
}

static void
HandleJRK(int keyin, int usb) {
  struct pollfd pfds[2] = {
    { .fd = usb, .events = POLLIN | POLLPRI, .revents = 0, },
    { .fd = keyin, .events = POLLIN | POLLPRI, .revents = 0, },
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

void FDSetNonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags < 0){
    throw std::runtime_error("couldn't get fd flags");
  }
  flags |= O_NONBLOCK;
  if(0 != fcntl(fd, F_SETFL, flags)){
    throw std::runtime_error("couldn't set fd flags");
  }
}

void FDSetRaw(int fd) {
  struct termios term;
  if(tcgetattr(fd, &term)){
    throw std::runtime_error("couldn't get serial settings");
  }
  term.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  if(tcsetattr(fd, TCSANOW, &term)){
    throw std::runtime_error("couldn't set serial raw");
  }
}

int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }

  // Open the USB serial device, and put it in raw mode
  const char* dev = argv[argc - 1];
  auto fd = open(dev, O_RDWR | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
  if(fd < 0){
    std::cerr << "couldn't open " << dev << ": " << strerror(errno) << std::endl;
    usage(std::cerr, EXIT_FAILURE);
  }

  // Disable terminal buffering on stdin, so we can get keypresses
  auto infd = STDIN_FILENO;
  FDSetNonblocking(infd);
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
  ReadJRKErrors(fd);
  ReadJRKTarget(fd);
  HandleJRK(infd, fd);

  // Restore terminal settings
  if(tcsetattr(infd, TCSANOW, &oldterm)){
    std::cerr << "couldn't restore terminal settings on " << infd << ": "
      << strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
