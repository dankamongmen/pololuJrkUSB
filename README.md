# PololuJrkUSB

A command-line tool for driving the Pololu Jrk USB motor controllers.
https://www.pololu.com/docs/pdf/0J38/jrk_motor_controller.pdf

## Hardware

The jrk provides bidirectional control of a DC brush motor via a USB mini-B
5-pin interface. It can be driven with open loop feedback, or a closed loop
using 0V–5V analog control or digital control up to 2MHz with a 1ms PID.
RC PWM, full-duplex TTL serial, and 0V–5V potentiometer control is also
available, though this program assumes USB serial. The control model is ACM,
supported by the cdc_acm Linux kernel module.

* [jrk 21v3](https://www.pololu.com/product/1392)
  * Vendor: 0x1ffb
  * Device: 0x0083
  * 5V–28V
  * 3A continuous, 5A peak
* [jrk 12v12](https://www.pololu.com/product/1393)
  * Vendor: 0x1ffb
  * Device: ????
  * 6V–16V
  * 12A continous, 30A peak

## Building

Run `make`.

### Dependencies

* GNU Make
* C++ compiler
* libreadline

## Usage

Launch the program with the USB serial device node as its argument for
interactive keyboard-driven use. The help text will be printed in response to
the 'help' command. Other commands include:

* 'feedback': Read feedback
* 'input': Read input
* 'target': Read target
* 'eflags': Read error flags

## Copyright and thanks

Copyright © Nick Black 2019.
Licensed under [Apache-2.0](https://www.apache.org/licenses/LICENSE-2.0).

Hardware and financial support were provided by
[Greenzie](https://www.greenzie.co/) of Atlanta, makers of autonomous
lawnmowers.

All errors are my own.
