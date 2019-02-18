#ifndef POLOLUJRKUSB_POLLER
#define POLOLUJRKUSB_POLLER

#include <queue>
#include <atomic>
#include <ostream>

namespace PololuJrkUSB {

class Poller {
public:
  Poller(const char* dev); // throws on failure to open device
  virtual ~Poller();
  void Poll();
  void ReadJRKInput(std::vector<std::string>::iterator begin,
                    std::vector<std::string>::iterator end);
  void ReadJRKFeedback(std::vector<std::string>::iterator begin,
                    std::vector<std::string>::iterator end);
  void ReadJRKTarget(std::vector<std::string>::iterator begin,
                    std::vector<std::string>::iterator end);
  void ReadJRKErrors(std::vector<std::string>::iterator begin,
                    std::vector<std::string>::iterator end);
  void StopPolling(std::vector<std::string>::iterator begin,
                    std::vector<std::string>::iterator end);

private:
  int devfd;
  std::atomic<bool> cancelled;
  std::queue<unsigned char> sent_cmds;

  std::ostream& HexOutput(std::ostream& s, const unsigned char* data, size_t len);
  int OpenDev(const char* dev);
  void SendJRKReadCommand(int cmd);
  void WriteJRKCommand(int cmd, int fd);
  void HandleUSB();

};

}

#endif
