#ifndef POLOLUJRKUSB_LIB_POLLER
#define POLOLUJRKUSB_LIB_POLLER

#include <queue>
#include <mutex>
#include <ostream>

namespace PololuJrkUSB {

constexpr unsigned PololuVendorID = 0x1ffb;
constexpr unsigned Jrk21v3ProductID = 0x0083;
constexpr unsigned Jrk12v12ProductID = 0x0085;

using PollerIOCallback = void(*)();

class Poller {
public:
  // Takes as parameter outcb a PollerIOCallback to fire after generating
  // output to std iostreams, to e.g. clean up readline prompts.
  Poller(const char* dev, PollerIOCallback outcb); // throws on failure to open
  virtual ~Poller();
  void Poll();
  void ReadJrkInput();
  void ReadJrkTarget();
  void ReadJrkFeedback();
  void ReadJrkScaledFeedback();
  void ReadJrkErrorSum();
  void ReadJrkDutyCycleTarget();
  void ReadJrkDutyCycle();
  void ReadJrkCurrent();
  void ReadJrkErrors();
  void SetJrkTarget(int target);
  void SetJrkOff();
  static std::ostream& HexOutput(std::ostream& s, const void* data, size_t len);

  // Direct the Poller to cease operating, but don't block on its actual exit
  void StopPolling();

private:
  int devfd;
  int cancelfd; // eventfd used for cancellation signal
  std::queue<unsigned char> sent_cmds;
  std::mutex lock; // guards sent_cmds, devfd, cancelfd
  PollerIOCallback iocallback;

  int OpenDev(const char* dev);
  void SendJRKReadCommand(int cmd);
  void WriteJRKCommand(int cmd, int fd);
  int USBToSigned16(uint16_t unsig);
  void HandleUSB();

};

}

#endif
