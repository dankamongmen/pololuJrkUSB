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

static void
KeyboardHelp(std::ostream& s) {
  s << "help: print this help text\n";
  s << "feedback: send a read feedback command\n";
  s << "target: send a read target command\n";
  s << "input: send a read input command\n";
  s << "eflags: send a read error flags command\n";
  s << std::endl;
}

class SplitException : public std::runtime_error {
public:
  SplitException(const std::string& what) :
    std::runtime_error(what) {}
};

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
