#ifndef POLOLUJRKUSB_LIB_USB
#define POLOLUJRKUSB_LIB_USB

#include <string>
#include <iostream>
#include <libusb.h>

namespace PololuJrkUSB {

void LibusbVersion(std::ostream& s);
void LibusbGetTopology(libusb_device* dev, int* bus, int* port);
void JrkGetFirmwareVersion(libusb_device_handle* dev);
void JrkGetSerialNumber(libusb_device_handle* dev,
                        const libusb_device_descriptor* desc);
void LibusbGetConfig(std::ostream& s, libusb_device_handle* dev);
void LibusbGetDesc(std::ostream& s, libusb_device_handle* dev,
                   const libusb_device_descriptor* desc);
std::string FindACMDevice(int bus, int port);

}

#endif
