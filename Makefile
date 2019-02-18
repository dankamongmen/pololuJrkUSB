.DELETE_ON_ERROR:
.PHONY: all bin clean
.DEFAULT_GOAL:=all

CFLAGS:=-W -Wall -Werror -pthread
LFLAGS:=-lreadline

BIN:=pololu
SRC:=$(shell find -type f -iname \*.cpp -print)
INC:=$(shell find -type f -iname \*.h -print)
OBJ:=$(SRC:%.cpp=%.o)

all: bin

bin: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ $(LFLAGS)

%.o: %.cpp $(INC)
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BIN) $(OBJ)
