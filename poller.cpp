#include <string>
#include <poll.h>
#include <cstring>
#include <fcntl.h>
#include <cassert>
#include <iomanip>
#include <unistd.h>
#include <iostream>
#include <termios.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include "poller.h"

namespace PololuJrkUSB {

constexpr unsigned char JRKCMD_READ_INPUT = 0xa1;
constexpr unsigned char JRKCMD_READ_FEEDBACK = 0xa3;
constexpr unsigned char JRKCMD_READ_TARGET = 0xa5;
constexpr unsigned char JRKCMD_READ_ERRORS = 0xb5;

int Poller::OpenDev(const char* dev) {
  auto fd = open(dev, O_RDWR | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
  if(fd < 0){
    throw std::runtime_error(std::string("couldn't open ") + dev + ": " + strerror(errno));
  }
  struct termios term;
  if(tcgetattr(fd, &term)){
    close(fd);
    throw std::runtime_error("couldn't get serial settings");
  }
  term.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  if(tcsetattr(fd, TCSANOW, &term)){
    close(fd);
    throw std::runtime_error("couldn't set serial raw");
  }
  return fd;
}

Poller::Poller(const char* dev) :
devfd(-1),
cancelled(false) {
    devfd = OpenDev(dev);
    std::cout << "Opened Pololu jrk " << dev << " at fd " << devfd << std::endl;
}

Poller::~Poller() {
  if(devfd >= 0){
    close(devfd); // FIXME warning on error?
    devfd = -1;
  }
}

void Poller::StopPolling(std::vector<std::string>::iterator begin,
                          std::vector<std::string>::iterator end) {
  (void)begin; (void)end; // FIXME
  cancelled.store(true);
  // FIXME interrupt poller
}

void Poller::WriteJRKCommand(int cmd, int fd) {
  assert(cmd >=0);
  assert(cmd < 0x100); // commands are a single byte
  unsigned char cmdbuf[1] = { (unsigned char)(cmd % 0x100u) };
  auto ss = ::write(fd, cmdbuf, sizeof(cmdbuf));
  if(ss < 0 || (size_t)ss < sizeof(cmdbuf)){
    std::cerr << "error writing command to " << fd << ": " << strerror(errno) << std::endl;
    // FIXME throw exception? hrmmmm
  }
}

void Poller::SendJRKReadCommand(int cmd) {
  // FIXME need to lock
  WriteJRKCommand(cmd, devfd);
  sent_cmds.push(cmd);
}

void Poller::ReadJRKInput(std::vector<std::string>::iterator begin,
                          std::vector<std::string>::iterator end) {
  (void)begin; (void)end; // FIXME
  constexpr unsigned char cmd = JRKCMD_READ_INPUT;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKFeedback(std::vector<std::string>::iterator begin,
                              std::vector<std::string>::iterator end) {
  (void)begin; (void)end; // FIXME
  constexpr auto cmd = JRKCMD_READ_FEEDBACK;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKTarget(std::vector<std::string>::iterator begin,
                            std::vector<std::string>::iterator end) {
  (void)begin; (void)end; // FIXME
  constexpr auto cmd = JRKCMD_READ_TARGET;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKErrors(std::vector<std::string>::iterator begin,
                            std::vector<std::string>::iterator end) {
  (void)begin; (void)end; // FIXME
  constexpr auto cmd = JRKCMD_READ_ERRORS;
  SendJRKReadCommand(cmd);
}

std::ostream& Poller::HexOutput(std::ostream& s, const unsigned char* data, size_t len) {
  std::ios state(NULL);
  state.copyfmt(s);
  s << std::hex;
  for(size_t i = 0 ; i < len ; ++i){
    s << std::setfill('0') << std::setw(2) << (int)data[i];
  }
  s.copyfmt(state);
  return s;
}

void Poller::HandleUSB() {
  constexpr auto bufsize = 2;
  unsigned char valbuf[bufsize];
  errno = 0;

  while((read(devfd, valbuf, bufsize)) == bufsize){
    int sword = valbuf[1] * 256 + valbuf[0];
    /*std::cout << "received bytes: 0x";
    HexOutput(std::cout, valbuf, sizeof(valbuf)) << " (" << sword << ")" << std::endl;*/
    if(sent_cmds.empty()){
      std::cerr << "warning: no outstanding command for recv" << std::endl;
      continue;
    }
    unsigned char expcmd = sent_cmds.front();
    sent_cmds.pop();
    switch(expcmd){
      case JRKCMD_READ_INPUT: std::cout << "Input is " << sword << std::endl; break;
      case JRKCMD_READ_FEEDBACK: std::cout << "Feedback is " << sword << std::endl; break;
      case JRKCMD_READ_TARGET: std::cout << "Target is " << sword << std::endl; break;
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
        std::cerr << "unexpected command " << (int)expcmd << std::endl;
    }
  }
  if(errno != EAGAIN){
    std::cerr << "error reading serial: " << strerror(errno) << std::endl;
    throw std::runtime_error("erp");
    // FIXME throw exception?
  }
}

// FIXME generalize for multiple devices
void Poller::Poll() {
  struct pollfd pfds[2] = {
    { .fd = devfd, .events = POLLIN | POLLPRI, .revents = 0, },
  };
  const auto nfds = sizeof(pfds) / sizeof(*pfds);
  while(!cancelled.load()){
    auto pret = poll(pfds, nfds, -1);
    if(pret < 0){
      std::cerr << "error polling " << nfds << " fds: " << strerror(errno) << std::endl;
      continue;
    }
    for(auto i = 0u ; i < nfds ; ++i){
      if(pfds[i].revents){
        if(pfds[i].fd == devfd){
          HandleUSB();
        }else{
          std::cout << "event on bogon fd " << pfds[i].fd << std::endl; // FIXME
        }
      }
    }
  }
}

}
