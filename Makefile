# Oh Shit! for Haiku and RenkuOS
#   make          builds OhShit (Haiku)
#   make package  builds ohshit-<version>-1-<arch>.hpkg (Haiku)
#   make tools    builds the test and helper tools (any POSIX system)

VERSION = 1.0.0

CXX ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wextra -std=c++17

REFMACHINE = src/z80/Z80.cpp src/video/Tms9918.cpp src/msx1/Psg.cpp \
	src/msx1/Machine1.cpp

GAME = src/game/Machine.cpp src/game/Bios.cpp src/game/Video.cpp \
	src/game/Main.cpp src/game/Sound.cpp src/game/Title.cpp \
	src/game/Player.cpp src/game/Ghosts.cpp src/game/Fruit.cpp \
	src/game/Level.cpp src/game/JoystickTest.cpp src/game/GameFile.cpp \
	src/video/Tms9918.cpp

AUDIO = src/audio/PsgSound.cpp
PLATFORM = src/platform/PlatformHaiku.cpp src/platform/PlatformCommon.cpp \
	src/platform/VideoScaler.cpp src/platform/KeyNames.cpp

TOOLS = osrun osnative game_diff osprof load_test

ifeq ($(shell uname -s),Haiku)
all: OhShit
else
all: tools
endif

OhShit: $(GAME) $(AUDIO) $(PLATFORM) src/main.cpp src/OhShit.rdef
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^) -lbe -lmedia -ltranslation -ldevice -ltracker
	rc -o OhShit.rsrc src/OhShit.rdef
	xres -o $@ OhShit.rsrc
	mimeset -f $@

# The Haiku package: the game in apps/OhShit and a Deskbar entry.
PACKAGE_DIR = build/package
APP_DIR = $(PACKAGE_DIR)/apps/OhShit
package: OhShit haiku/PackageInfo.in
	rm -rf $(PACKAGE_DIR)
	mkdir -p $(APP_DIR) $(PACKAGE_DIR)/data/deskbar/menu/Applications
	cp OhShit README.md LICENSE $(APP_DIR)/
	ln -s ../../../../apps/OhShit/OhShit \
		"$(PACKAGE_DIR)/data/deskbar/menu/Applications/Oh Shit!"
	sed -e "s/@VERSION@/$(VERSION)/" -e "s/@ARCH@/$$(getarch 2>/dev/null || uname -m)/" \
		haiku/PackageInfo.in > $(PACKAGE_DIR)/.PackageInfo
	package create -C $(PACKAGE_DIR) \
		"$(CURDIR)/ohshit-$(VERSION)-1-$$(getarch 2>/dev/null || uname -m).hpkg"
	@echo "Install with: pkgman install ./ohshit-$(VERSION)-1-*.hpkg"

icon: tools/icon/make_icon.py
	python3 tools/icon/make_icon.py data/icon src/OhShit.rdef

tools: $(TOOLS)

osrun: tools/osrun.cpp $(REFMACHINE) src/msx1/*.h src/z80/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osrun.cpp $(REFMACHINE)

osprof: tools/osprof.cpp $(REFMACHINE) src/msx1/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osprof.cpp $(REFMACHINE)

osnative: tools/osnative.cpp $(GAME) $(AUDIO) src/game/*.h src/video/*.h
	$(CXX) $(CXXFLAGS) -o $@ tools/osnative.cpp $(GAME) $(AUDIO)

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
	rm -f $(TOOLS) OhShit OhShit.rsrc
	rm -rf $(PACKAGE_DIR)

.PHONY: all tools clean symbols test package icon
