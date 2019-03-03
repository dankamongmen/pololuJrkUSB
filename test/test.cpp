#include <thread>
#include <iostream>
#include <unistd.h>
#include "poller.h"

using namespace PololuJrkUSB;

int main(void){
  Poller p("/dev/ttyACM0", nullptr);
  std::thread usb(&Poller::Poll, std::ref(p));
  while(1){
    p.ReadJrkInput();
    p.ReadJrkTarget();
    p.ReadJrkFeedback();
    p.ReadJrkScaledFeedback();
    p.ReadJrkErrorSum();
    p.ReadJrkDutyCycleTarget();
    p.ReadJrkDutyCycle();
    p.ReadJrkErrors();
    std::cout << "wrote command suite" << std::endl;
    usleep(100);
  }
  usb.join();
  return 0;
}
