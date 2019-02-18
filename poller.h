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
  void ReadJRKFeedback();
  void ReadJRKTarget();
  void ReadJRKErrors();
  void SetJRKTarget(int target);
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
  void HandleUSB();

};

}

#endif
