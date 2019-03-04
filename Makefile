.DELETE_ON_ERROR:
.PHONY: all bin clean
.DEFAULT_GOAL:=all

OUT:=.out
LIB:=lib
BIN:=$(addprefix $(OUT)/, pololu loadtest)
LIBSRC:=$(wildcard $(LIB)/*.cpp)
LIBINC:=$(wildcard $(LIB)/*.h)
LIBOBJ:=$(addprefix $(OUT)/, $(LIBSRC:%.cpp=%.o))

CFLAGS:=-W -Wall -Werror -pthread $(shell pkg-config --cflags libusb-1.0) -I$(LIB)
LFLAGS:=-lreadline $(shell pkg-config --libs libusb-1.0)

all: bin

bin: $(BIN)

$(OUT)/pololu: pololu/pololu.cpp $(LIBOBJ) $(LIBINC)
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -o $@ $< $(LIBOBJ) $(LFLAGS)

$(OUT)/loadtest: test/loadtest.cpp $(LIBOBJ) $(LIBINC)
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -o $@ $< $(LIBOBJ) $(LFLAGS)

$(OUT)/%.o: %.cpp $(LIBINC)
	@mkdir -p $(@D)
	$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BIN) $(OBJ)
