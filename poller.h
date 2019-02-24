#ifndef POLOLUJRKUSB_POLLER
#define POLOLUJRKUSB_POLLER

#include <queue>
#include <mutex>
#include <atomic>
#include <ostream>

namespace PololuJrkUSB {

constexpr unsigned PololuVendorID = 0x1ffb;
constexpr unsigned Jrk21v3ProductID = 0x0083;
constexpr unsigned Jrk12v12ProductID = 0x0085;

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
  static std::ostream& HexOutput(std::ostream& s, const void* data, size_t len);

private:
  int devfd;
  std::atomic<bool> cancelled;
  std::queue<unsigned char> sent_cmds;
  std::mutex lock; // guards sent_cmds and devfd

  int OpenDev(const char* dev);
  void SendJRKReadCommand(int cmd);
  void WriteJRKCommand(int cmd, int fd);
  int USBToSigned16(uint16_t unsig);
  void HandleUSB();

};

}

#endif
