#ifndef POLOLUJRKUSB_POLLER
#define POLOLUJRKUSB_POLLER

#include <queue>
#include <mutex>
#include <atomic>
#include <ostream>

namespace PololuJrkUSB {

class Poller {
public:
  Poller(const char* dev); // throws on failure to open device
  virtual ~Poller();
  void Poll();
  void ReadJRKInput();
  void ReadJRKTarget();
  void ReadJRKFeedback();
  void ReadJRKScaledFeedback();
  void ReadJRKErrorSum();
  void ReadJRKDutyCycleTarget();
  void ReadJRKDutyCycle();
  void ReadJRKErrors();
  void SetJRKTarget(int target);
  void SetJRKOff();
  void StopPolling();

private:
  int devfd;
  std::atomic<bool> cancelled;
  std::queue<unsigned char> sent_cmds;
  std::mutex lock; // guards sent_cmds and devfd

  std::ostream& HexOutput(std::ostream& s, const unsigned char* data, size_t len);
  int OpenDev(const char* dev);
  void SendJRKReadCommand(int cmd);
  void WriteJRKCommand(int cmd, int fd);
  int USBToSigned16(uint16_t unsig);
  void HandleUSB();

};

}

#endif
