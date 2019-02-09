.DELETE_ON_ERROR:
.PHONY: all bin clean
.DEFAULT_GOAL:=all

BIN:=pololu

all: bin

bin: $(BIN)

$(BIN): $(BIN).cpp
	$(CXX) $(CFLAGS) -o $@ $< $(LFLAGS)

clean:
	rm -rf $(BIN)
