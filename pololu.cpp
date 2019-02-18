#include <queue>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "poller.h"

static void
usage(std::ostream& os, int ret) {
  os << "usage: pololu dev\n";
  os << std::endl;
  exit(ret);
}

static void
KeyboardHelp(std::ostream& s) {
  s << "help: print this help text\n";
  s << "feedback: send a read feedback command\n";
  s << "target: send a read target command\n";
  s << "input: send a read input command\n";
  s << "eflags: send a read error flags command\n";
  s << std::endl;
}

#define ANSI_WHITE "\033[1;37m"
#define RL_START "\x01" // RL_PROMPT_START_IGNORE
#define RL_END "\x02"   // RL_PROMPT_END_IGNORE

static void
ReadlineLoop(PololuJrkUSB::Poller& poller) {
  char* line;
  while(1){
    line = readline(RL_START "\033[0;35m" RL_END
      "[" RL_START "\033[0;36m" RL_END
      "catena" RL_START "\033[0;35m" RL_END
      "] " RL_START ANSI_WHITE RL_END);
    if(line == nullptr){
      break;
    }
    // FIXME tokenize read line
    add_history(line);
    (void)poller; // FIXME run command
    free(line);
  }
}

// FIXME it looks like we can maybe get firmware version with 0x060100
int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }

  // Open the USB serial device, and put it in raw, nonblocking mode
  const char* dev = argv[argc - 1];
  auto poller = PololuJrkUSB::Poller(dev);

  KeyboardHelp(std::cout);
  poller.ReadJRKErrors();
  poller.ReadJRKTarget();
  std::thread usb(&PololuJrkUSB::Poller::Poll, std::ref(poller));
  ReadlineLoop(poller);

  return EXIT_SUCCESS;
}
