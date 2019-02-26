#include <queue>
#include <string>
#include <thread>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <libusb.h>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "libusb.h"
#include "poller.h"

using namespace std::literals::string_literals;

static void
usage(std::ostream& os, int ret) {
  os << "usage: pololu dev\n";
  os << std::endl;
  exit(ret);
}

static bool cancelled = false;

class SplitException : public std::runtime_error {
public:
  SplitException(const std::string& what) :
    std::runtime_error(what) {}
};

// FIXME rewrite all these with a function object
static void ReadJRKInput(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKInput();
}

static void ReadJRKFeedback(PololuJrkUSB::Poller& poller,
                     std::vector<std::string>::iterator begin,
                     std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKFeedback();
}

static void ReadJRKTarget(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKTarget();
}

static void ReadJRKScaledFeedback(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKScaledFeedback();
}

static void ReadJRKErrorSum(PololuJrkUSB::Poller& poller,
                     std::vector<std::string>::iterator begin,
                     std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKErrorSum();
}

static void ReadJRKDutyCycleTarget(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKDutyCycleTarget();
}

static void ReadJRKDutyCycle(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKDutyCycle();
}

static void SetJRKTarget(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin == end || begin + 1 != end){
    std::cerr << "command requires a single argument [0..4095]" << std::endl;
    return;
  }
  auto target = std::stoi(*begin);
  poller.SetJRKTarget(target);
}

static void ReadJRKErrors(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKErrors();
}

static void SetJRKOff(PololuJrkUSB::Poller& poller,
               std::vector<std::string>::iterator begin,
               std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.SetJRKOff();
}

static void StopPolling(PololuJrkUSB::Poller& poller,
                 std::vector<std::string>::iterator begin,
                 std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.StopPolling();
  cancelled = true;
}

// Split a line into whitespace-delimited tokens, supporting simple quoting
// using single quotes, plus escaping using backslash.
static std::vector<std::string>
SplitInput(const char* line) {
  std::vector<std::string> tokens;
  std::vector<char> token;
  bool quoted = false;
  bool escaped = false;
  int offset = 0;
  char c;
  while( (c = line[offset]) ){
    if(c == '\\' && !escaped){
      escaped = true;
    }else if(escaped){
      token.push_back(c);
      escaped = false;
    }else if(quoted){
      if(c == '\''){
        quoted = false;
      }else{
        token.push_back(c);
      }
    }else if(isspace(c)){
      if(token.size()){
        tokens.emplace_back(std::string(token.begin(), token.end()));
        token.clear();
      }
    }else if(c == '\''){
      quoted = true;
    }else{
      token.push_back(c);
    }
    ++offset;
  }
  if(token.size()){
    tokens.emplace_back(std::string(token.begin(), token.end()));
  }
  if(quoted){
    throw SplitException("unterminated single quote");
  }
  return tokens;
}

#define ANSI_WHITE "\033[1;37m"
#define ANSI_GREY "\033[0;37m"
#define RL_START "\x01" // RL_PROMPT_START_IGNORE
#define RL_END "\x02"   // RL_PROMPT_END_IGNORE

static void
ReadlineLoop(PololuJrkUSB::Poller& poller) {
  const struct {
    const std::string cmd;
    void (* fxn)(PololuJrkUSB::Poller&,
                 std::vector<std::string>::iterator,
                 std::vector<std::string>::iterator);
    const char* help;
  } cmdtable[] = {
    { .cmd = "quit", .fxn = &StopPolling, .help = "exit program", },
    { .cmd = "feedback", .fxn = &ReadJRKFeedback, .help = "send a read feedback request", },
    { .cmd = "target", .fxn = &ReadJRKTarget, .help = "send a read target request", },
    { .cmd = "input", .fxn = &ReadJRKInput, .help = "send a read input command", },
    { .cmd = "sfeedback", .fxn = &ReadJRKScaledFeedback, .help = "send a read scaled feedback request", },
    { .cmd = "errorsum", .fxn = &ReadJRKErrorSum, .help = "send a read error sum request", },
    { .cmd = "cycletarg", .fxn = &ReadJRKDutyCycleTarget, .help = "send a read duty cycle target command", },
    { .cmd = "cycle", .fxn = &ReadJRKDutyCycle, .help = "send a read duty cycle command", },
    { .cmd = "eflags", .fxn = &ReadJRKErrors, .help = "send a read error flags command", },
    { .cmd = "settarget", .fxn = &SetJRKTarget, .help = "send set target command (arg: [0..4095])", },
    { .cmd = "off", .fxn = &SetJRKOff, .help = "send a motor off command", },
    { .cmd = "", .fxn = nullptr, .help = "", },
  }, *c;
  char* line;
  while(!cancelled){
    line = readline(RL_START "\033[0;35m" RL_END
      "[" RL_START "\033[0;36m" RL_END
      "pololu" RL_START "\033[0;35m" RL_END
      "] " RL_START ANSI_WHITE RL_END);
    if(line == nullptr){
      poller.StopPolling();
      break;
    }
    std::vector<std::string> tokes;
    try{
      tokes = SplitInput(line);
    }catch(SplitException& e){
      std::cerr << e.what() << std::endl;
      add_history(line);
      free(line);
      continue;
    }
    if(tokes.size() == 0){
      free(line);
      continue;
    }
    add_history(line);
    for(c = cmdtable ; c->fxn ; ++c){
      if(c->cmd == tokes[0]){
        (c->fxn)(poller, tokes.begin() + 1, tokes.end());
        break;
      }
    }
    if(c->fxn == nullptr && tokes[0] != "help"){
      std::cerr << "unknown command: " << tokes[0] << std::endl;
    }else if(c->fxn == nullptr){ // display help
      for(c = cmdtable ; c->fxn ; ++c){
        std::cout << c->cmd << ANSI_GREY " " << c->help << ANSI_WHITE "\n";
      }
      std::cout << "help" ANSI_GREY ": list commands" ANSI_WHITE << std::endl;
    }
    free(line);
  }
}

// Return 0 to rearm the callback, or 1 to disable it.
static int
libusb_callback(libusb_context *ctx, libusb_device *dev,
                libusb_hotplug_event event, void *user_data) {
  (void)ctx; (void)user_data;
  if(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED != event){
    std::cerr << "unexpected libusb event " << event << std::endl;
  }else{
    struct libusb_device_descriptor desc;
    auto ret = libusb_get_device_descriptor(dev, &desc);
    if(ret){
      throw std::runtime_error("error describing usb device: "s +
                               libusb_strerror(static_cast<libusb_error>(ret)));
    }
    if(desc.idVendor != PololuJrkUSB::PololuVendorID){
      std::cerr << "unexpected idVendor " << desc.idVendor << std::endl;
    }else if(desc.idProduct != PololuJrkUSB::Jrk21v3ProductID &&
             desc.idProduct != PololuJrkUSB::Jrk12v12ProductID){
      std::cout << "unsupported idProduct " << desc.idProduct << std::endl;
    }else{
      libusb_device_handle* handle;
      if( (ret = libusb_open(dev, &handle)) ){
        throw std::runtime_error("error opening usb device: "s +
                                libusb_strerror(static_cast<libusb_error>(ret)));
      }
      int bus, port;
      PololuJrkUSB::LibusbGetTopology(dev, &bus, &port);
      try{
        auto devtty = PololuJrkUSB::FindACMDevice(bus, port);
        std::cout << " Found control TTY: " << devtty << std::endl;
      }catch(std::runtime_error& e){ // FIXME clamp down on acceptable errors
        std::cerr << "Couldn't find control TTY: " << e.what() << std::endl;
      }
      PololuJrkUSB::LibusbGetDesc(std::cout, handle, &desc);
      PololuJrkUSB::JrkGetSerialNumber(handle, &desc);
      PololuJrkUSB::JrkGetFirmwareVersion(handle);
      PololuJrkUSB::LibusbGetConfig(std::cout, handle);
      libusb_close(handle); // FIXME
    }
  }
  return 0;
}

static void
PollerReadlineCallback() {
  rl_forced_update_display();
}

// FIXME it looks like we can maybe get firmware version with 0x060100
int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }

  PololuJrkUSB::LibusbVersion(std::cout);
  libusb_context* usbctx;
  if(libusb_init(&usbctx)){
    std::cerr << "error initializing libusb" << std::endl; // FIXME details?
    return EXIT_FAILURE;
  }
  // Register a callback for any Pololu device that is already registered, or
  // happens to show up (FIXME handle departures). We filter by productID
  // within the callback proper
  libusb_hotplug_callback_handle cbhandle;
  auto ret = libusb_hotplug_register_callback(usbctx,
                                   LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                   LIBUSB_HOTPLUG_ENUMERATE,
                                   PololuJrkUSB::PololuVendorID,
                                   LIBUSB_HOTPLUG_MATCH_ANY, // productID
                                   LIBUSB_HOTPLUG_MATCH_ANY, // class
                                   libusb_callback, nullptr, &cbhandle);
  if(ret){
    throw std::runtime_error("error registering libusb callback: "s +
                             libusb_strerror(static_cast<libusb_error>(ret)));
  }

  // Open the USB serial device, and put it in raw, nonblocking mode
  const char* dev = argv[argc - 1];
  PololuJrkUSB::Poller poller(dev, PollerReadlineCallback);

  poller.ReadJRKErrors();
  poller.ReadJRKTarget();
  std::thread usb(&PololuJrkUSB::Poller::Poll, std::ref(poller));
  ReadlineLoop(poller);
  std::cout << "Joining USB poller thread..." << std::endl;
  usb.join();

  libusb_hotplug_deregister_callback(usbctx, cbhandle);
  libusb_exit(usbctx);

  return EXIT_SUCCESS;
}
