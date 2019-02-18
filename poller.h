#ifndef POLOLUJRKUSB_POLLER
#define POLOLUJRKUSB_POLLER

#include <queue>

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

private:
  int devfd;
  std::queue<unsigned char> sent_cmds;

  int OpenDev(const char* dev);
  void SendJRKReadCommand(int cmd);
  void WriteJRKCommand(int cmd, int fd);
  void HandleUSB();

};

}

#endif
