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
#include <sys/eventfd.h>
#include "poller.h"

using namespace std::literals::string_literals;

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

// USB control transfers
constexpr unsigned char JRKUSB_GET_PARAMETER = 0x81;

// Taken from https://github.com/pololu/pololu-usb-sdk.git/Jrk/Jrk/Jrk_protocol.cs
enum class JrkConfigParam {
  PARAMETER_INITIALIZED = 0, // 1 bit boolean value
  PARAMETER_INPUT_MODE = 1, // 1 byte unsigned value.  Valid values are INPUT_MODE_*.  Init parameter.
  PARAMETER_INPUT_MINIMUM = 2, // 2 byte unsigned value (0-4095)
  PARAMETER_INPUT_MAXIMUM = 6, // 2 byte unsigned value (0-4095)
  PARAMETER_OUTPUT_MINIMUM = 8, // 2 byte unsigned value (0-4095)
  PARAMETER_OUTPUT_NEUTRAL = 10, // 2 byte unsigned value (0-4095)
  PARAMETER_OUTPUT_MAXIMUM = 12, // 2 byte unsigned value (0-4095)
  PARAMETER_INPUT_INVERT = 16, // 1 bit boolean value
  PARAMETER_INPUT_SCALING_DEGREE = 17, // 1 bit boolean value
  PARAMETER_INPUT_POWER_WITH_AUX = 18, // 1 bit boolean value
  PARAMETER_INPUT_ANALOG_SAMPLES_EXPONENT = 20, // 1 byte unsigned value, 0-8 - averages together 4 * 2^x samples
  PARAMETER_INPUT_DISCONNECT_MINIMUM = 22, // 2 byte unsigned value (0-4095)
  PARAMETER_INPUT_DISCONNECT_MAXIMUM = 24, // 2 byte unsigned value (0-4095)
  PARAMETER_INPUT_NEUTRAL_MAXIMUM = 26, // 2 byte unsigned value (0-4095)
  PARAMETER_INPUT_NEUTRAL_MINIMUM = 28, // 2 byte unsigned value (0-4095)

  PARAMETER_SERIAL_MODE = 30, // 1 byte unsigned value.  Valid values are SERIAL_MODE_*.  MUST be SERIAL_MODE_USB_DUAL_PORT if INPUT_MODE!=INPUT_MODE_SERIAL.  Init variable.
  PARAMETER_SERIAL_FIXED_BAUD_RATE = 31, // 2-byte unsigned value; 0 means autodetect.  Init parameter.
  PARAMETER_SERIAL_TIMEOUT = 34, // 2-byte unsigned value
  PARAMETER_SERIAL_ENABLE_CRC = 36, // 1 bit boolean value
  PARAMETER_SERIAL_NEVER_SUSPEND = 37, // 1 bit boolean value
  PARAMETER_SERIAL_DEVICE_NUMBER = 38, // 1 byte unsigned value, 0-127

  PARAMETER_FEEDBACK_MODE = 50, // 1 byte unsigned value.  Valid values are FEEDBACK_MODE_*.  Init parameter.
  PARAMETER_FEEDBACK_MINIMUM = 51, // 2 byte unsigned value
  PARAMETER_FEEDBACK_MAXIMUM = 53, // 2 byte unsigned value
  PARAMETER_FEEDBACK_INVERT = 55, // 1 bit boolean value
  PARAMETER_FEEDBACK_POWER_WITH_AUX = 57, // 1 bit boolean value
  PARAMETER_FEEDBACK_DEAD_ZONE = 58, // 1 byte unsigned value
  PARAMETER_FEEDBACK_ANALOG_SAMPLES_EXPONENT = 59, // 1 byte unsigned value, 0-8 - averages together 4 * 2^x samples
  PARAMETER_FEEDBACK_DISCONNECT_MINIMUM = 61, // 2 byte unsigned value (0-4095)
  PARAMETER_FEEDBACK_DISCONNECT_MAXIMUM = 63, // 2 byte unsigned value (0-4095)

  PARAMETER_PROPORTIONAL_MULTIPLIER = 70, // 2 byte unsigned value (0-1023)
  PARAMETER_PROPORTIONAL_EXPONENT = 72, // 1 byte unsigned value (0-15)
  PARAMETER_INTEGRAL_MULTIPLIER = 73, // 2 byte unsigned value (0-1023)
  PARAMETER_INTEGRAL_EXPONENT = 75, // 1 byte unsigned value (0-15)
  PARAMETER_DERIVATIVE_MULTIPLIER = 76, // 2 byte unsigned value (0-1023)
  PARAMETER_DERIVATIVE_EXPONENT = 78, // 1 byte unsigned value (0-15)
  PARAMETER_PID_PERIOD = 79, // 2 byte unsigned value
  PARAMETER_PID_INTEGRAL_LIMIT = 81, // 2 byte unsigned value
  PARAMETER_PID_RESET_INTEGRAL = 84, // 1 bit boolean value

  PARAMETER_MOTOR_PWM_FREQUENCY = 100, // 1 byte unsigned value.  Valid values are MOTOR_PWM_FREQUENCY.  Init parameter.
  PARAMETER_MOTOR_INVERT = 101, // 1 bit boolean value

  // WARNING: The EEPROM initialization assumes the 5 parameters below are consecutive!
  PARAMETER_MOTOR_MAX_DUTY_CYCLE_WHILE_FEEDBACK_OUT_OF_RANGE = 102, // 2 byte unsigned value (0-600)
  PARAMETER_MOTOR_MAX_ACCELERATION_FORWARD = 104, // 2 byte unsigned value (1-600)
  PARAMETER_MOTOR_MAX_ACCELERATION_REVERSE = 106, // 2 byte unsigned value (1-600)
  PARAMETER_MOTOR_MAX_DUTY_CYCLE_FORWARD = 108, // 2 byte unsigned value (0-600)
  PARAMETER_MOTOR_MAX_DUTY_CYCLE_REVERSE = 110, // 2 byte unsigned value (0-600)
  // WARNING: The EEPROM initialization assumes the 5 parameters above are consecutive!

  // WARNING: The EEPROM initialization assumes the 2 parameters below are consecutive!
  PARAMETER_MOTOR_MAX_CURRENT_FORWARD = 112, // 1 byte unsigned value (units of current_calibration_forward)
  PARAMETER_MOTOR_MAX_CURRENT_REVERSE = 113, // 1 byte unsigned value (units of current_calibration_reverse)
  // WARNING: The EEPROM initialization assumes the 2 parameters above are consecutive!

  // WARNING: The EEPROM initialization assumes the 2 parameters below are consecutive!
  PARAMETER_MOTOR_CURRENT_CALIBRATION_FORWARD = 114, // 1 byte unsigned value (units of mA)
  PARAMETER_MOTOR_CURRENT_CALIBRATION_REVERSE = 115, // 1 byte unsigned value (units of mA)
  // WARNING: The EEPROM initialization assumes the 2 parameters above are consecutive!

  PARAMETER_MOTOR_BRAKE_DURATION_FORWARD = 116, // 1 byte unsigned value (units of 5 ms)
  PARAMETER_MOTOR_BRAKE_DURATION_REVERSE = 117, // 1 byte unsigned value (units of 5 ms)
  PARAMETER_MOTOR_COAST_WHEN_OFF = 118, // 1 bit boolean value (coast=1, brake=0)

  PARAMETER_ERROR_ENABLE = 130, // 2 byte unsigned value.  See below for the meanings of the bits.
  PARAMETER_ERROR_LATCH = 132, // 2 byte unsigned value.  See below for the meanings of the bits.
};

int Poller::OpenDev(const char* dev) {
  auto fd = open(dev, O_RDWR | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
  if(fd < 0){
    throw std::runtime_error("couldn't open "s + dev + ": " + strerror(errno));
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

Poller::Poller(const char* dev, PollerIOCallback outcb) :
devfd(-1),
cancelfd(-1),
iocallback(outcb) {
  devfd = OpenDev(dev);
  cancelfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if(cancelfd == -1){
    close(devfd);
    throw std::runtime_error("couldn't open eventfd: "s + strerror(errno));
  }
  std::cout << "Opened Pololu jrk " << dev << " at fd " << devfd << std::endl;
}

Poller::~Poller() {
  if(devfd >= 0){
    if(close(devfd)){
      std::cerr << "error closing device fd: " << strerror(errno) << std::endl;
    }
  }
  if(cancelfd >= 0){
    if(close(cancelfd)){
      std::cerr << "error closing cancel fd: " << strerror(errno) << std::endl;
    }
  }
}

void Poller::StopPolling() {
  std::lock_guard<std::mutex> guard(lock);
  uint64_t events = 1;
  auto ret = ::write(cancelfd, &events, sizeof(events));
  if(ret < 0){
    throw std::runtime_error("couldn't write to cancelfd: "s + strerror(errno));
  }
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

void Poller::ReadJrkInput() {
  constexpr unsigned char cmd = JRKCMD_READ_INPUT;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkFeedback() {
  constexpr auto cmd = JRKCMD_READ_FEEDBACK;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkScaledFeedback() {
  constexpr auto cmd = JRKCMD_READ_SCALED_FEEDBACK;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkTarget() {
  constexpr auto cmd = JRKCMD_READ_TARGET;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkErrorSum() {
  constexpr auto cmd = JRKCMD_READ_ERRORSUM;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkDutyCycleTarget() {
  constexpr auto cmd = JRKCMD_READ_DUTY_TARGET;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkDutyCycle() {
  constexpr auto cmd = JRKCMD_READ_DUTY;
  SendJRKReadCommand(cmd);
}

void Poller::ReadJrkErrors() {
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

std::ostream& Poller::HexOutput(std::ostream& s, const void* data, size_t len) {
  std::ios state(NULL);
  state.copyfmt(s);
  s << std::hex;
  for(size_t i = 0 ; i < len ; ++i){
    s << std::setfill('0') << std::setw(2) <<
      (int)(static_cast<const unsigned char*>(data)[i]);
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
          ((uword & 0x0001) ? "AwaitingCmd " : "") <<
          ((uword & 0x0002) ? "NoPower " : "") <<
          ((uword & 0x0004) ? "DriveError " : "") <<
          ((uword & 0x0008) ? "InvalidInput " : "") <<
          ((uword & 0x0010) ? "InputDisconn " : "") <<
          ((uword & 0x0020) ? "FdbckDisconn " : "") <<
          ((uword & 0x0040) ? "AmpsExceeded " : "") <<
          ((uword & 0x0080) ? "SerialSig " : "") <<
          ((uword & 0x0100) ? "UARTOflow " : "") <<
          ((uword & 0x0200) ? "SerialOflow " : "") <<
          ((uword & 0x0400) ? "SerialCRC " : "") <<
          ((uword & 0x0800) ? "SerialProto " : "") <<
          ((uword & 0x1000) ? "TimeoutRX " : "") <<
          ((uword == 0) ? "None" : "") << std::endl;
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
  struct pollfd pfds[] = {
    { .fd = devfd, .events = POLLIN | POLLPRI, .revents = 0, },
    { .fd = cancelfd, .events = POLLIN | POLLPRI, .revents = 0, },
  };
  const auto nfds = sizeof(pfds) / sizeof(*pfds);
  bool cancelled = false;
  while(!cancelled){
    auto pret = poll(pfds, nfds, -1);
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
          if(iocallback){
            iocallback();
          }
        }else if(pfds[i].fd == cancelfd){
          cancelled = true; // don't need to read it to know what it means
        }else{
          std::cout << "event on bogon fd " << pfds[i].fd << std::endl; // FIXME
        }
      }
    }
  }
}

}
