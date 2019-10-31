#include <array>
#include <cstring>
#include <iostream>
#include <libusb.h>
#include <dirent.h>
#include <arpa/inet.h>
#include "poller.h"
#include "usb.h"

using namespace std::literals::string_literals;

namespace PololuJrkUSB {

// Used bmRequestTypes. Device-to-Host (MSB) is always set.
constexpr uint8_t BMREQ_STANDARD = 0x80; // firmware version
constexpr uint8_t BMREQ_VENDOR = 0xc0; // config and variables

// USB control transfers sent to JRK_RECIPIENT_CONFIG
constexpr unsigned char JRKUSB_GET_PARAMETER = 0x81;
constexpr unsigned char JRKUSB_GET_VARIABLES = 0x83;

void JrkGetSerialNumber(libusb_device_handle* dev, const libusb_device_descriptor* desc) {
  if(desc->iSerialNumber){
    std::array<unsigned char, BUFSIZ> serialbuf;
    auto ret = libusb_get_string_descriptor_ascii(dev, desc->iSerialNumber,
                  serialbuf.data(), serialbuf.size());
    if(ret <= 0){
      throw std::runtime_error("error extracting serialno: "s +
                             libusb_strerror(static_cast<libusb_error>(ret)));
    }
    std::cout << " Serial number: " << serialbuf.data() << std::endl;
  }
}

void JrkGetFirmwareVersion(libusb_device_handle* dev) {
  constexpr int FIRMWARE_RESPLEN = 14;
  constexpr int FIRMWARE_OFFSET = 12;
  std::array<unsigned char, FIRMWARE_RESPLEN> buffer;
  auto ret = libusb_control_transfer(dev, BMREQ_STANDARD, 6, 0x0100, 0,
                                     buffer.data(), buffer.size(), 0);
  if(ret < 0 || static_cast<size_t>(ret) != buffer.size()){
    throw std::runtime_error("error extracting firmware: "s +
                             libusb_strerror(static_cast<libusb_error>(ret)));
  }
  auto minor = buffer[FIRMWARE_OFFSET] & 0xf;
  auto major = ((buffer[FIRMWARE_OFFSET] >> 4) & 0xf) +
    ((buffer[FIRMWARE_OFFSET + 1] >> 4) & 0xf) * 100;
  std::cout << " Firmware version: " << major << "." << minor << std::endl;
}

// FIXME theoretically, there can be a series of ports, but we only ever seem
// to see one from libusb_get_port_numbers(), even when hubs are chained...
void LibusbGetTopology(libusb_device* dev, int* bus, int* port) {
  const int USB_TOPOLOGY_MAXLEN = 7;
  std::array<uint8_t, USB_TOPOLOGY_MAXLEN> numbers;
  *bus = libusb_get_bus_number(dev);
  auto ret = libusb_get_port_numbers(dev, numbers.data(), numbers.size());
  if(ret <= 0){
    throw std::runtime_error("error locating usb device: "s +
                             libusb_strerror(static_cast<libusb_error>(ret)));
  }
  *port = numbers[0]; // FIXME only ever seem to see one...
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

void LibusbGetConfig(std::ostream& s, libusb_device_handle* dev) {
  unsigned char data[2]; // maximum number of bytes used for any value
  for(auto& param : JrkParams){
    // FIXME maybe want timeouts on control transfer?
    int ret = libusb_control_transfer(dev, BMREQ_VENDOR, JRKUSB_GET_PARAMETER, 0,
                        static_cast<uint8_t>(param.id), data, param.bytes, 0);
    if(ret <= 0){
      throw std::runtime_error("error reading from usb device: "s +
                               libusb_strerror(static_cast<libusb_error>(ret)));
    }
    s << " " << param.name << ": 0x";
    PololuJrkUSB::Poller::HexOutput(s, data, param.bytes) << std::endl;
  }
}

void LibusbGetDesc(std::ostream& s, libusb_device_handle* dev,
              const libusb_device_descriptor* desc) {
  s << " VendorID: ";
  uint16_t id = ntohs(desc->idVendor);
  PololuJrkUSB::Poller::HexOutput(s, &id, sizeof(id)) << " ProductID: ";
  id = ntohs(desc->idProduct);
  PololuJrkUSB::Poller::HexOutput(s, &id, sizeof(id));
  if(desc->iProduct){
    std::array<unsigned char, BUFSIZ> name;
    auto ret = libusb_get_string_descriptor_ascii(dev, desc->iProduct, name.data(), name.size());
    if(ret > 0){
      s << " (" << name.data() << ')';
    }
  }
  s << std::endl;
}

std::string FindACMDevice(int bus, int port) {
  auto dir = "/sys/bus/usb/devices/"s + std::to_string(bus) + "-" +
    std::to_string(port) + ":1.0/tty";
  auto dd = opendir(dir.c_str());
  if(dd == nullptr){
    throw std::runtime_error("error opening "s + dir + ": " + strerror(errno));
  }
  struct dirent* dent;
  errno = 0;
  while( (dent = readdir(dd)) ){
    if(strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..")){
      break;
    }
    errno = 0;
  }
  if(errno){
    closedir(dd);
    throw std::runtime_error("error enumerating "s + dir + ": " + strerror(errno));
  }
  if(dent == nullptr){
    closedir(dd);
    throw std::runtime_error("no device in "s + dir);
  }
  std::string dev = dent->d_name;
  closedir(dd);
  return dev;
}

void LibusbVersion(std::ostream& s) {
  auto ver = libusb_get_version();
  s << "libusb version " << ver->major << "." << ver->minor << "." << ver->micro << std::endl;
}

}
