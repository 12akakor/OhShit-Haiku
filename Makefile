# Oh Shit! for Haiku

CXX ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wextra -std=c++17

REFMACHINE = src/z80/Z80.cpp src/msx1/Tms9918.cpp src/msx1/Psg.cpp \
	src/msx1/Machine1.cpp

TOOLS = osrun

all: tools

tools: $(TOOLS)

osrun: tools/osrun.cpp $(REFMACHINE) src/msx1/*.h src/z80/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osrun.cpp $(REFMACHINE)

clean:
	rm -f $(TOOLS)

.PHONY: all tools clean
