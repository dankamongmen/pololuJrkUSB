A command-line tool for driving the Pololu Jrk USB motor controllers

https://www.pololu.com/docs/pdf/0J38/jrk_motor_controller.pdf

The jrk provides bidirectional control of a DC brush motor via a USB mini-B
5-pin interface. It can be driven with open loop feedback, or a closed loop
using 0V--5V analog control or digital control up to 2MHz with a 1ms PID.
RC PWM, full-duplex TTL serial, and 0V--5V potentiometer control is also
available, though this program assumes USB serial. The control model is ACM,
supported by the cdc_acm Linux kernel module.

jrk 21v3: 5V--28V, 3A continuous, 5A peak
jrk 12v12: 6V--16V, 12A continous, 30A peak
