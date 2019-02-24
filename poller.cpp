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
#include <readline/readline.h>
#include "poller.h"

namespace PololuJrkUSB {

// Serial commands using the "compact protocol" (i.e. non daisy-chained)
constexpr unsigned char JRKCMD_READ_INPUT = 0xa1;
constexpr unsigned char JRKCMD_READ_TARGET = 0xa3;
constexpr unsigned char JRKCMD_READ_FEEDBACK = 0xa5;
constexpr unsigned char JRKCMD_READ_SCALED_FEEDBACK = 0xa7;
constexpr unsigned char JRKCMD_READ_ERRORSUM = 0xa9;
constexpr unsigned char JRKCMD_READ_DUTY_TARGET = 0xab;
constexpr unsigned char JRKCMD_READ_DUTY = 0xad;
constexpr unsigned char JRKCMD_READ_PIDCOUNT = 0xb1;
constexpr unsigned char JRKCMD_READ_ERRORS = 0xb5;
constexpr unsigned char JRKCMD_MOTOR_OFF = 0xff;

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

void Poller::StopPolling() {
  cancelled.store(true);
  // FIXME interrupt poller, and we can stop ticking in poll()
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
  std::lock_guard<std::mutex> guard(lock);
  WriteJRKCommand(cmd, devfd);
  sent_cmds.push(cmd);
}

void Poller::ReadJRKInput() {
  constexpr unsigned char cmd = JRKCMD_READ_INPUT;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKFeedback() {
  constexpr auto cmd = JRKCMD_READ_FEEDBACK;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKScaledFeedback() {
  constexpr auto cmd = JRKCMD_READ_SCALED_FEEDBACK;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKTarget() {
  constexpr auto cmd = JRKCMD_READ_TARGET;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKErrorSum() {
  constexpr auto cmd = JRKCMD_READ_ERRORSUM;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKDutyCycleTarget() {
  constexpr auto cmd = JRKCMD_READ_DUTY_TARGET;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKDutyCycle() {
  constexpr auto cmd = JRKCMD_READ_DUTY;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJRKErrors() {
  constexpr auto cmd = JRKCMD_READ_ERRORS;
  SendJRKReadCommand(cmd);
}

void Poller::SetJRKTarget(int target) {
  std::lock_guard<std::mutex> guard(lock);
  if(target < 0 || target > 4095){
    std::cerr << "invalid target " << target << std::endl;
    return; // FIXME throw exception?
  }
  unsigned char cmdbuf[] = {
    (unsigned char)(0xC0 + (target & 0x1F)),
    (unsigned char)((target >> 5) & 0x7F),
  };
  auto ss = ::write(devfd, cmdbuf, sizeof(cmdbuf));
  if(ss < 0 || (size_t)ss < sizeof(cmdbuf)){
    std::cerr << "error writing to " << devfd << ": " << strerror(errno) << std::endl;
    return; // FIXME throw exception? hrmmmm
  }
}

void Poller::SetJRKOff() {
  std::lock_guard<std::mutex> guard(lock);
  constexpr auto cmd = JRKCMD_MOTOR_OFF;
  WriteJRKCommand(cmd, devfd); // no reply, so don't use SendJRKReadCommand
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

int Poller::USBToSigned16(uint16_t unsig) {
  int16_t sig;
  static_assert(sizeof(unsig) == sizeof(sig));
  memcpy(&sig, &unsig, sizeof(sig));
  return sig;
}

void Poller::HandleUSB() {
  constexpr auto bufsize = 2;
  unsigned char valbuf[bufsize];
  errno = 0;

  // FIXME save readline state
  while((read(devfd, valbuf, bufsize)) == bufsize){
    unsigned uword = valbuf[1] * 256 + valbuf[0];
    /* std::cout << "received bytes: 0x";
    HexOutput(std::cout, valbuf, sizeof(valbuf)) << " (" << uword << ")" << std::endl; */
    if(sent_cmds.empty()){
      std::cerr << "warning: no outstanding command for recv" << std::endl;
      continue;
    }
    unsigned char expcmd = sent_cmds.front();
    sent_cmds.pop();
    switch(expcmd){
      case JRKCMD_READ_INPUT: std::cout << "Input is " << uword << std::endl; break;
      case JRKCMD_READ_TARGET: std::cout << "Target is " << uword << std::endl; break;
      case JRKCMD_READ_FEEDBACK: std::cout << "Feedback is " << uword << std::endl; break;
      case JRKCMD_READ_SCALED_FEEDBACK: std::cout << "Scaled feedback is " << uword << std::endl; break;
      case JRKCMD_READ_ERRORSUM: std::cout << "Error sum (integral) is " << USBToSigned16(uword) << std::endl; break;
      case JRKCMD_READ_DUTY_TARGET: std::cout << "Duty cycle target is " << USBToSigned16(uword) << std::endl; break;
      case JRKCMD_READ_DUTY: std::cout << "Duty cycle is " << USBToSigned16(uword) << std::endl; break;
      case JRKCMD_READ_ERRORS:
        std::cout << "Error bits: " <<
          ((uword & 0x0001) ? "AwaitingCmd" : "") <<
          ((uword & 0x0002) ? "NoPower" : "") <<
          ((uword & 0x0004) ? "DriveError" : "") <<
          ((uword & 0x0008) ? "InvalidInput" : "") <<
          ((uword & 0x0010) ? "InputDisconn" : "") <<
          ((uword & 0x0020) ? "FdbckDisconn" : "") <<
          ((uword & 0x0040) ? "AmpsExceeded" : "") <<
          ((uword & 0x0080) ? "SerialSig" : "") <<
          ((uword & 0x0100) ? "UARTOflow" : "") <<
          ((uword & 0x0200) ? "SerialOflow" : "") <<
          ((uword & 0x0400) ? "SerialCRC" : "") <<
          ((uword & 0x0800) ? "SerialProto" : "") <<
          ((uword & 0x1000) ? "TimeoutRX" : "") <<
          ((uword == 0) ? "None" : "") <<
          std::endl;
        break;
      default:
        std::cerr << "unexpected command " << (int)expcmd << std::endl;
    }
  }
  // FIXME restore readline state
  if(errno != EAGAIN){
    std::cerr << "error reading serial: " << strerror(errno) << std::endl;
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
    // FIXME should not need to tick, but need signal from StopPolling()
    auto pret = poll(pfds, nfds, 100);
    if(pret < 0){
      std::cerr << "error polling " << nfds << " fds: " << strerror(errno) << std::endl;
      continue;
    }
    for(auto i = 0u ; i < nfds ; ++i){
      if(pfds[i].revents){
        if(pfds[i].fd == devfd){
          lock.lock();
          HandleUSB();
          lock.unlock();
        }else{
          std::cout << "event on bogon fd " << pfds[i].fd << std::endl; // FIXME
        }
      }
    }
  }
}

}
