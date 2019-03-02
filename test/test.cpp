#include <thread>
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
    //sleep(1);
  }
  usb.join();
  return 0;
}
