#include <queue>
#include <string>
#include <thread>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <libusb.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "poller.h"

constexpr unsigned PololuVendorID = 0x1ffb;

static void
usage(std::ostream& os, int ret) {
  os << "usage: pololu dev\n";
  os << std::endl;
  exit(ret);
}

static bool cancelled = false;

class SplitException : public std::runtime_error {
public:
  SplitException(const std::string& what) :
    std::runtime_error(what) {}
};

// FIXME rewrite all these with a function object
static void ReadJRKInput(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKInput();
}

static void ReadJRKFeedback(PololuJrkUSB::Poller& poller,
                     std::vector<std::string>::iterator begin,
                     std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKFeedback();
}

static void ReadJRKTarget(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKTarget();
}

static void ReadJRKScaledFeedback(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKScaledFeedback();
}

static void ReadJRKErrorSum(PololuJrkUSB::Poller& poller,
                     std::vector<std::string>::iterator begin,
                     std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKErrorSum();
}

static void ReadJRKDutyCycleTarget(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKDutyCycleTarget();
}

static void ReadJRKDutyCycle(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKDutyCycle();
}

static void SetJRKTarget(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin == end || begin + 1 != end){
    std::cerr << "command requires a single argument [0..4095]" << std::endl;
    return;
  }
  auto target = std::stoi(*begin);
  poller.SetJRKTarget(target);
}

static void ReadJRKErrors(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKErrors();
}

static void SetJRKOff(PololuJrkUSB::Poller& poller,
               std::vector<std::string>::iterator begin,
               std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.SetJRKOff();
}

static void StopPolling(PololuJrkUSB::Poller& poller,
                 std::vector<std::string>::iterator begin,
                 std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.StopPolling();
  cancelled = true;
}

// Split a line into whitespace-delimited tokens, supporting simple quoting
// using single quotes, plus escaping using backslash.
static std::vector<std::string>
SplitInput(const char* line) {
  std::vector<std::string> tokens;
  std::vector<char> token;
  bool quoted = false;
  bool escaped = false;
  int offset = 0;
  char c;
  while( (c = line[offset]) ){
    if(c == '\\' && !escaped){
      escaped = true;
    }else if(escaped){
      token.push_back(c);
      escaped = false;
    }else if(quoted){
      if(c == '\''){
        quoted = false;
      }else{
        token.push_back(c);
      }
    }else if(isspace(c)){
      if(token.size()){
        tokens.emplace_back(std::string(token.begin(), token.end()));
        token.clear();
      }
    }else if(c == '\''){
      quoted = true;
    }else{
      token.push_back(c);
    }
    ++offset;
  }
  if(token.size()){
    tokens.emplace_back(std::string(token.begin(), token.end()));
  }
  if(quoted){
    throw SplitException("unterminated single quote");
  }
  return tokens;
}

#define ANSI_WHITE "\033[1;37m"
#define ANSI_GREY "\033[0;37m"
#define RL_START "\x01" // RL_PROMPT_START_IGNORE
#define RL_END "\x02"   // RL_PROMPT_END_IGNORE

static void
ReadlineLoop(PololuJrkUSB::Poller& poller) {
  const struct {
    const std::string cmd;
    void (* fxn)(PololuJrkUSB::Poller&,
                 std::vector<std::string>::iterator,
                 std::vector<std::string>::iterator);
    const char* help;
  } cmdtable[] = {
    { .cmd = "quit", .fxn = &StopPolling, .help = "exit program", },
    { .cmd = "feedback", .fxn = &ReadJRKFeedback, .help = "send a read feedback request", },
    { .cmd = "target", .fxn = &ReadJRKTarget, .help = "send a read target request", },
    { .cmd = "input", .fxn = &ReadJRKInput, .help = "send a read input command", },
    { .cmd = "sfeedback", .fxn = &ReadJRKScaledFeedback, .help = "send a read scaled feedback request", },
    { .cmd = "errorsum", .fxn = &ReadJRKErrorSum, .help = "send a read error sum request", },
    { .cmd = "cycletarg", .fxn = &ReadJRKDutyCycleTarget, .help = "send a read duty cycle target command", },
    { .cmd = "cycle", .fxn = &ReadJRKDutyCycle, .help = "send a read duty cycle command", },
    { .cmd = "eflags", .fxn = &ReadJRKErrors, .help = "send a read error flags command", },
    { .cmd = "settarget", .fxn = &SetJRKTarget, .help = "send set target command (arg: [0..4095])", },
    { .cmd = "off", .fxn = &SetJRKOff, .help = "send a motor off command", },
    { .cmd = "", .fxn = nullptr, .help = "", },
  }, *c;
  char* line;
  while(!cancelled){
    line = readline(RL_START "\033[0;35m" RL_END
      "[" RL_START "\033[0;36m" RL_END
      "pololu" RL_START "\033[0;35m" RL_END
      "] " RL_START ANSI_WHITE RL_END);
    if(line == nullptr){
      poller.StopPolling();
      break;
    }
    std::vector<std::string> tokes;
    try{
      tokes = SplitInput(line);
    }catch(SplitException& e){
      std::cerr << e.what() << std::endl;
      add_history(line);
      free(line);
      continue;
    }
    if(tokes.size() == 0){
      free(line);
      continue;
    }
    add_history(line);
    for(c = cmdtable ; c->fxn ; ++c){
      if(c->cmd == tokes[0]){
        (c->fxn)(poller, tokes.begin() + 1, tokes.end());
        break;
      }
    }
    if(c->fxn == nullptr && tokes[0] != "help"){
      std::cerr << "unknown command: " << tokes[0] << std::endl;
    }else if(c->fxn == nullptr){ // display help
      for(c = cmdtable ; c->fxn ; ++c){
        std::cout << c->cmd << ANSI_GREY " " << c->help << ANSI_WHITE "\n";
      }
      std::cout << "help" ANSI_GREY ": list commands" ANSI_WHITE << std::endl;
    }
    free(line);
  }
}

static void
LibusbVersion(std::ostream& s) {
  auto ver = libusb_get_version();
  s << "libusb version " << ver->major << "." << ver->minor << "." << ver->micro << std::endl;
}

// Used bmRequestTypes. Device-to-Host (MSB) is always set.
constexpr uint8_t BMREQ_STANDARD = 0x80; // firmware version
constexpr uint8_t BMREQ_VENDOR = 0xc0; // config and variables

// USB control transfers sent to JRK_RECIPIENT_CONFIG
constexpr unsigned char JRKUSB_GET_PARAMETER = 0x81;
constexpr unsigned char JRKUSB_GET_VARIABLES = 0x83;

static void
JrkGetSerialNumber(libusb_device_handle* dev, const libusb_device_descriptor* desc) {
  if(desc->iSerialNumber){
    std::array<unsigned char, 256> serialbuf;
    auto ret = libusb_get_string_descriptor_ascii(dev, desc->iSerialNumber,
                  serialbuf.data(), serialbuf.size());
    if(ret <= 0){
      throw std::runtime_error(std::string("error extracting serialno: ") +
                             libusb_strerror(static_cast<libusb_error>(ret)));
    }
    std::cout << " Serial number: " << serialbuf.data() << std::endl;
  }
}

static void
JrkGetFirmwareVersion(libusb_device_handle* dev) {
  constexpr int FIRMWARE_RESPLEN = 14;
  constexpr int FIRMWARE_OFFSET = 12;
  std::array<unsigned char, FIRMWARE_RESPLEN> buffer;
  auto ret = libusb_control_transfer(dev, BMREQ_STANDARD, 6, 0x0100, 0,
                                     buffer.data(), buffer.size(), 0);
  if(ret != buffer.size()){
    throw std::runtime_error(std::string("error extracting firmware: ") +
                             libusb_strerror(static_cast<libusb_error>(ret)));
  }
  auto minor = buffer[FIRMWARE_OFFSET] & 0xf;
  auto major = ((buffer[FIRMWARE_OFFSET] >> 4) & 0xf) +
    ((buffer[FIRMWARE_OFFSET + 1] >> 4) & 0xf) * 100;
  std::cout << " Firmware version: " << major << "." << minor << std::endl;
}

static void
LibusbGetTopology(libusb_device* dev) {
  const int USB_TOPOLOGY_MAXLEN = 7;
  std::array<uint8_t, USB_TOPOLOGY_MAXLEN> numbers;
  int bus = libusb_get_bus_number(dev);
  auto ret = libusb_get_port_numbers(dev, numbers.data(), numbers.size());
  if(ret <= 0){
    throw std::runtime_error(std::string("error locating usb device: ") +
                             libusb_strerror(static_cast<libusb_error>(ret)));
  }
  std::cout << "USB device at " << bus << "-";
  for(auto n = 0 ; n < ret ; ++n){
    std::cout << static_cast<int>(numbers[n]) << (n + 1 < ret ? "." : "");
  }
  std::cout << std::endl;
}

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

static struct {
  const char* name;
  JrkConfigParam id;
  int bytes; // how many bytes for value
} JrkParams[] = {
  // We want 0 here, oddly enough
  { .name = "Initialized", .id = JrkConfigParam::PARAMETER_INITIALIZED, .bytes = 1, } ,
  // We want 0, INPUT_MODE_SERIAL
  { .name = "Input mode", .id = JrkConfigParam::PARAMETER_INPUT_MODE, .bytes = 1, } ,
  // We want 0, SERIAL_MODE_USB_DUAL_PORT
  { .name = "Serial mode", .id = JrkConfigParam::PARAMETER_SERIAL_MODE, .bytes = 1, } ,
  // 0 is autodetect, otherwise fixed baud rate
  { .name = "Serial baud", .id = JrkConfigParam::PARAMETER_SERIAL_FIXED_BAUD_RATE, .bytes = 2, } ,
  // 0 means CRC7 is not expected, 1 means it is
  { .name = "CRC7", .id = JrkConfigParam::PARAMETER_SERIAL_ENABLE_CRC, .bytes = 1, } ,
};

static void
LibusbGetConfig(libusb_device_handle* dev) {
  unsigned char data[2]; // maximum number of bytes used for any value
  for(auto& param : JrkParams){
    // FIXME maybe want timeouts on control transfer?
    int ret = libusb_control_transfer(dev, BMREQ_VENDOR, JRKUSB_GET_PARAMETER, 0,
                        static_cast<uint8_t>(param.id), data, param.bytes, 0);
    if(ret <= 0){
      throw std::runtime_error(std::string("error reading from usb device: ") +
                               libusb_strerror(static_cast<libusb_error>(ret)));
    }
    std::cout << " " << param.name << ": 0x";
    PololuJrkUSB::Poller::HexOutput(std::cout, data, param.bytes) << std::endl;
  }
}

// Return 0 to rearm the callback, or 1 to disable it.
static int
libusb_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event,
                void *user_data __attribute__ ((unused))) {
  (void)ctx; // FIXME
  if(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED != event){
    std::cerr << "unexpected libusb event " << event << std::endl; // FIXME throw?
  }else{
    struct libusb_device_descriptor desc;
    auto ret = libusb_get_device_descriptor(dev, &desc);
    if(ret){
      throw std::runtime_error(std::string("error describing usb device: ") +
                               libusb_strerror(static_cast<libusb_error>(ret)));
    }else if(desc.idVendor != PololuVendorID){
      std::cerr << "unexpected idVendor " << desc.idVendor << std::endl; // FIXME throw?
    }
    libusb_device_handle* handle;
    if( (ret = libusb_open(dev, &handle)) ){
      throw std::runtime_error(std::string("error opening usb device: ") +
                               libusb_strerror(static_cast<libusb_error>(ret)));
    }
    LibusbGetTopology(dev);
    JrkGetSerialNumber(handle, &desc);
    JrkGetFirmwareVersion(handle);
    LibusbGetConfig(handle);
    libusb_close(handle); // FIXME
  }
  return 0;
}

// FIXME it looks like we can maybe get firmware version with 0x060100
int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }

  LibusbVersion(std::cout);
  libusb_context* usbctx;
  if(libusb_init(&usbctx)){
    std::cerr << "error initializing libusb" << std::endl; // FIXME details?
  }
  // FIXME should maybe limit the product IDs we handle?
  auto ret = libusb_hotplug_register_callback(usbctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                   LIBUSB_HOTPLUG_ENUMERATE, PololuVendorID,
                                   LIBUSB_HOTPLUG_MATCH_ANY, // productID
                                   LIBUSB_HOTPLUG_MATCH_ANY, // class
                                   libusb_callback, nullptr, nullptr);
  if(ret){
    throw std::runtime_error(std::string("registering libusb callback: ") +
                             libusb_strerror(static_cast<libusb_error>(ret)));
  }
  // FIXME find Jrks via libusb via get_descriptor(LIBUSB_DT_DEVICE)

  // Open the USB serial device, and put it in raw, nonblocking mode
  const char* dev = argv[argc - 1];
  PololuJrkUSB::Poller poller(dev);

  poller.ReadJRKErrors();
  poller.ReadJRKTarget();
  std::thread usb(&PololuJrkUSB::Poller::Poll, std::ref(poller));
  ReadlineLoop(poller);
  std::cout << "Joining USB poller thread..." << std::endl;
  usb.join();

  libusb_exit(usbctx);

  return EXIT_SUCCESS;
}
