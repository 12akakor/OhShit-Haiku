# Oh Shit! for Haiku

CXX ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wextra -std=c++17

REFMACHINE = src/z80/Z80.cpp src/video/Tms9918.cpp src/msx1/Psg.cpp \
	src/msx1/Machine1.cpp

GAME = src/game/Machine.cpp src/game/Bios.cpp src/game/Video.cpp \
	src/game/Main.cpp src/game/Sound.cpp src/game/Title.cpp \
	src/game/Player.cpp src/game/Ghosts.cpp src/game/Fruit.cpp \
	src/game/Level.cpp src/game/JoystickTest.cpp src/game/GameFile.cpp \
	src/video/Tms9918.cpp

TOOLS = osrun osnative game_diff osprof load_test

all: tools

tools: $(TOOLS)

osrun: tools/osrun.cpp $(REFMACHINE) src/msx1/*.h src/z80/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osrun.cpp $(REFMACHINE)

osprof: tools/osprof.cpp $(REFMACHINE) src/msx1/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osprof.cpp $(REFMACHINE)

osnative: tools/osnative.cpp $(GAME) src/game/*.h src/video/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osnative.cpp $(GAME)

game_diff: tests/game_diff.cpp $(GAME) $(REFMACHINE) src/game/*.h src/msx1/*.h
	$(CXX) $(CXXFLAGS) -o $@ tests/game_diff.cpp $(GAME) src/z80/Z80.cpp src/msx1/Psg.cpp src/msx1/Machine1.cpp

load_test: tests/load_test.cpp $(GAME) src/game/*.h
	$(CXX) $(CXXFLAGS) -o $@ tests/load_test.cpp $(GAME)

symbols: tools/dis/names.txt tools/dis/blocks.txt
	python3 tools/dis/gen_symbols.py tools/dis/names.txt tools/dis/blocks.txt src/game/Symbols.h

# Differential test of the whole program (needs C-BIOS and a game file).
GAME_FILE ?= OhShit.rom
test: game_diff load_test
	./load_test $(GAME_FILE)
	./game_diff $(GAME_FILE) 30000 -seed 1 -warp 1500 -poke 150 -esc -quiet
	./game_diff $(GAME_FILE) 30000 -seed 2 -warp 1500 -poke 150 -quiet

clean:
	rm -f $(TOOLS)

.PHONY: all tools clean symbols test
