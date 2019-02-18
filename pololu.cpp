#include <queue>
#include <string>
#include <thread>
#include <cstdio>
#include <vector>
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

static bool cancelled = false;

class SplitException : public std::runtime_error {
public:
  SplitException(const std::string& what) :
    std::runtime_error(what) {}
};

void ReadJRKInput(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKInput();
}

void ReadJRKFeedback(PololuJrkUSB::Poller& poller,
                     std::vector<std::string>::iterator begin,
                     std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKFeedback();
}

void ReadJRKTarget(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKTarget();
}

void SetJRKTarget(PololuJrkUSB::Poller& poller,
                  std::vector<std::string>::iterator begin,
                  std::vector<std::string>::iterator end) {
  if(begin == end || begin + 1 != end){
    std::cerr << "command requires a single argument [0..4095]" << std::endl;
    return;
  }
  auto target = std::stoi(*begin);
  poller.SetJRKTarget(target);
}

void ReadJRKErrors(PololuJrkUSB::Poller& poller,
                   std::vector<std::string>::iterator begin,
                   std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.ReadJRKErrors();
}

void SetJRKOff(PololuJrkUSB::Poller& poller,
               std::vector<std::string>::iterator begin,
               std::vector<std::string>::iterator end) {
  if(begin != end){
    std::cerr << "command does not accept options" << std::endl;
    return;
  }
  poller.SetJRKOff();
}

void StopPolling(PololuJrkUSB::Poller& poller,
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

// FIXME it looks like we can maybe get firmware version with 0x060100
int main(int argc, const char** argv) {
  if(argc != 2){
    usage(std::cerr, EXIT_FAILURE);
  }

  // Open the USB serial device, and put it in raw, nonblocking mode
  const char* dev = argv[argc - 1];
  PololuJrkUSB::Poller poller(dev);

  poller.ReadJRKErrors();
  poller.ReadJRKTarget();
  std::thread usb(&PololuJrkUSB::Poller::Poll, std::ref(poller));
  ReadlineLoop(poller);
  std::cout << "Joining USB poller thread..." << std::endl;
  usb.join();

  return EXIT_SUCCESS;
}
