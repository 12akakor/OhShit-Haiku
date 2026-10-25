/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Differential test of the whole program: the original runs on the MSX1
// reference machine, the rewrite natively, in step.
//
//   game_diff game.rom segments [-seed n] [-warp n] [-quiet]
//
// -warp n: every n passes of the main loop, set the number of eaten dots
// to one short of the end of the maze (both sides), to reach later mazes
// and the intermissions.
// -poke n: about every n passes, one of: the fruit appears soon, the
// player jumps onto the fruit or onto a pill, a ghost jumps onto the
// player (both sides the same), so eating fruit and ghosts is tested.
//
// The two sides meet at sync points (Init, Title, the title loop, NewGame,
// every pass of the main loop). There the program state is compared:
// RAM #8107-#EFFF, JIFFY, video memory, VDP registers 0-7, PSG registers
// and whether interrupts are enabled. Interrupts only happen at the sync
// points, on both sides, as many as the original's frames took; that keeps
// the two sides in the same order of events. The original runs one
// segment ahead, so its LD A,R values can be replayed in the rewrite.
// Input: random keys and joystick moves, the same on both sides for a
// segment. On a difference the native state is copied from the original,
// so later differences are found too.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../src/game/Bios.h"
#include "../src/game/Game.h"
#include "../src/game/GameFile.h"
#include "../src/game/Timing.h"
#include "../src/msx1/Machine1.h"


namespace {

struct Done {};

std::vector<uint8_t>
ReadFile(const char* path)
{
	std::vector<uint8_t> data;
	FILE* f = fopen(path, "rb");
	if (f == NULL)
		return data;
	int c;
	while ((c = fgetc(f)) != EOF)
		data.push_back(static_cast<uint8_t>(c));
	fclose(f);
	return data;
}


class Reference {
public:
	Machine1	m;
	uint64_t	time = 0;
	uint64_t	handlerTime = 0;	// cycles spent in delivered interrupts
	uint64_t	handlerTotal = 0;
	long		interrupts = 0;
	uint64_t	nextFrame = Machine1::kCyclesPerFrame;
	int			frames = 0;			// frames since the last sync
	std::deque<uint8_t> refresh;	// LD A,R values of the segment ahead

	// Profile (game_diff -profile): exclusive cycles per routine, as osprof.
	bool		profile = false;
	struct Frame { uint16_t entry; uint16_t sp; uint64_t cycles; uint16_t last; };
	std::map<uint32_t, std::pair<uint64_t, uint64_t> > exits;	// sum, count
	std::vector<Frame> stack;
	std::map<uint16_t, uint64_t> exclusive;
	std::map<uint16_t, uint64_t> calls;
	bool		inHandler = false;

	// One instruction; frame ends set the VDP flag. With 'interrupts' the
	// CPU takes them as usual (start up only).
	void Step(bool interrupts)
	{
		Z80& cpu = m.Cpu();
		if (interrupts && m.Vdp().InterruptPending() && cpu.r.iff1 && !cpu.eiPending) {
			time += static_cast<uint64_t>(cpu.Interrupt());
		} else {
			bool ldAR = m.Read(cpu.r.pc) == 0xed
				&& m.Read(static_cast<uint16_t>(cpu.r.pc + 1)) == 0x5f;
			uint16_t pc = cpu.r.pc;
			uint16_t sp = cpu.r.sp;
			uint8_t op = m.Read(pc);
			int spent = cpu.Step();
			time += static_cast<uint64_t>(spent);
			if (ldAR)
				refresh.push_back(cpu.r.a);
			if (profile && !inHandler) {
				if (!stack.empty()) {
					exclusive[stack.back().entry] += static_cast<uint64_t>(spent);
					stack.back().cycles += static_cast<uint64_t>(spent);
					stack.back().last = pc;
				} else
					exclusive[0xffff] += static_cast<uint64_t>(spent);
				bool isCall = op == 0xcd || (op & 0xc7) == 0xc4 || (op & 0xc7) == 0xc7;
				if (isCall && cpu.r.sp == static_cast<uint16_t>(sp - 2)) {
					stack.push_back(Frame{ cpu.r.pc, sp, 0, cpu.r.pc });
					calls[cpu.r.pc]++;
				}
				while (!stack.empty() && cpu.r.sp >= stack.back().sp) {
					auto& e = exits[(static_cast<uint32_t>(stack.back().entry) << 16)
						| stack.back().last];
					e.first += stack.back().cycles;
					e.second++;
					stack.pop_back();
				}
			}
		}
		if (time >= nextFrame) {
			nextFrame += Machine1::kCyclesPerFrame;
			m.Vdp().StartVBlank();
			frames++;
		}
	}

	// Runs (no interrupts) until one of the sync addresses; returns it.
	int RunToSync(const std::set<uint16_t>& syncs, uint64_t maxCycles)
	{
		uint64_t limit = time + maxCycles;
		Step(false);
		while (time < limit) {
			uint16_t pc = m.Cpu().r.pc;
			if (syncs.count(pc))
				return pc;
			if (m.Cpu().r.halted) {
				// HALT with interrupts suppressed: let one through
				return -2;
			}
			Step(false);
		}
		return -1;
	}

	// Delivers one interrupt and runs the handler to its end.
	void Interrupt()
	{
		Z80& cpu = m.Cpu();
		if (!cpu.r.iff1)
			return;
		uint16_t pc = cpu.r.pc;
		uint16_t sp = cpu.r.sp;
		uint64_t start = time;
		inHandler = true;
		time += static_cast<uint64_t>(cpu.Interrupt());
		long guard = 0;
		while (!(cpu.r.pc == pc && cpu.r.sp == sp) && ++guard < 10000000)
			Step(false);
		inHandler = false;
		handlerTime += time - start;
		handlerTotal += time - start;
		interrupts++;
	}
};


Reference gRef;
std::set<uint16_t> gSyncs;
long gChecks = 0, gFailures = 0;
bool gInInterrupt = false;
long gSyncCount[6];
bool gQuiet = false;


void
CopyToNative()
{
	memcpy(gMem, gRef.m.Ram(), 0x10000);
	memcpy(gVdp.VramWritable(), gRef.m.Vdp().Vram(), 0x4000);
	for (int i = 0; i < 8; i++)
		gVdp.WriteRegister(i, gRef.m.Vdp().Register(i));
	uint8_t* psg = PsgRegisters();
	for (int i = 0; i < 16; i++)
		psg[i] = gRef.m.GetPsg().Register(i);
	gClock.interruptsEnabled = gRef.m.Cpu().r.iff1;
}


bool
Compare(const char* what)
{
	gChecks++;
	std::string report;
	char buf[160];
	const uint8_t* ram = gRef.m.Ram();
	int ramDiffs = 0;
	for (int a = 0x8107; a < 0xf000; a++) {
		if (a >= 0xa392 && a < 0xa500)
			continue;
		if (ram[a] != gMem[a] && ramDiffs++ < 6) {
			snprintf(buf, sizeof(buf), " RAM %04X ref %02X port %02X;", a, ram[a], gMem[a]);
			report += buf;
		}
	}
	for (int a = 0xfc9e; a <= 0xfc9f; a++) {
		if (ram[a] != gMem[a]) {
			snprintf(buf, sizeof(buf), " JIFFY+%d ref %02X port %02X;", a - 0xfc9e,
				ram[a], gMem[a]);
			report += buf;
		}
	}
	const uint8_t* vram = gRef.m.Vdp().Vram();
	int vramDiffs = 0;
	for (int a = 0; a < 0x4000; a++) {
		if (vram[a] != gVdp.Vram()[a] && vramDiffs++ < 6) {
			snprintf(buf, sizeof(buf), " VRAM %04X ref %02X port %02X;", a, vram[a],
				gVdp.Vram()[a]);
			report += buf;
		}
	}
	for (int i = 0; i < 8; i++) {
		if (gRef.m.Vdp().Register(i) != gVdp.Register(i)) {
			snprintf(buf, sizeof(buf), " R%d ref %02X port %02X;", i,
				gRef.m.Vdp().Register(i), gVdp.Register(i));
			report += buf;
		}
	}
	for (int i = 0; i < 16; i++) {
		if (i == 14)
			continue;
		if (gRef.m.GetPsg().Register(i) != PsgRegisters()[i]) {
			snprintf(buf, sizeof(buf), " PSG%d ref %02X port %02X;", i,
				gRef.m.GetPsg().Register(i), PsgRegisters()[i]);
			report += buf;
		}
	}
	if (gRef.m.Cpu().r.iff1 != gClock.interruptsEnabled) {
		snprintf(buf, sizeof(buf), " IFF ref %d port %d;", gRef.m.Cpu().r.iff1,
			gClock.interruptsEnabled);
		report += buf;
	}
	if (report.empty())
		return true;
	if (gFailures++ < 100)
		printf("FAIL %s:%s (RAM %d, VRAM %d)\n", what, report.c_str(), ramDiffs, vramDiffs);
	return false;
}


const char*
SyncName(uint16_t pc)
{
	switch (pc) {
		case sym::code::Init: return "Init";
		case sym::code::Title: return "Title";
		case 0x8aee: return "TitleLoop";
		case sym::code::NewGame: return "NewGame";
		case sym::code::MainLoop: return "MainLoop";
		case 0x89c6: return "HiscoreName";
		case 0x8a23: return "HiscoreRelease";
		case 0x841a: case 0x841f: case 0x8424: return "Pause";
		default: return "?";
	}
}

}	// namespace


int
main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: game_diff game.rom segments [-seed n] [-quiet]\n");
		return 2;
	}
	long segments = atol(argv[2]);
	unsigned seed = 1;
	long warp = 0;
	long poke = 0;
	bool escape = false;
	bool timing = false;
	bool profile = false;
	static std::map<uint16_t, uint64_t> nativeCost, nativeCalls;
	for (int i = 3; i < argc; i++) {
		if (!strcmp(argv[i], "-seed") && i + 1 < argc)
			seed = static_cast<unsigned>(atol(argv[++i]));
		else if (!strcmp(argv[i], "-poke") && i + 1 < argc)
			poke = atol(argv[++i]);
		else if (!strcmp(argv[i], "-warp") && i + 1 < argc)
			warp = atol(argv[++i]);
		else if (!strcmp(argv[i], "-esc"))
			escape = true;
		else if (!strcmp(argv[i], "-profile"))
			profile = true;
		else if (!strcmp(argv[i], "-timing"))
			timing = true;
		else if (!strcmp(argv[i], "-quiet"))
			gQuiet = true;
	}
	srand(seed);

	std::vector<uint8_t> rom = ReadFile(argv[1]);
	static GameImage image;
	std::string error;
	if (!LoadGameFile(rom, &image, &error)) {
		fprintf(stderr, "%s\n", error.c_str());
		return 2;
	}
	if (!gRef.m.LoadBios(ReadFile("/usr/share/cbios/cbios_main_msx1.rom"),
			ReadFile("/usr/share/cbios/cbios_logo_msx1.rom"))
		|| !gRef.m.LoadCartridge(rom)) {
		fprintf(stderr, "cannot set up the reference machine\n");
		return 2;
	}
	gRef.m.Reset();
	InstallGameImage(image);

	// Start up: the original runs freely to Init.
	for (long guard = 0; gRef.m.Cpu().r.pc != sym::code::Init && guard < 100000000; guard++)
		gRef.Step(true);
	if (gRef.m.Cpu().r.pc != sym::code::Init) {
		fprintf(stderr, "the original did not reach Init\n");
		return 2;
	}
	gRef.frames = 0;
	CopyToNative();
	gClock.deliverInterrupts = false;
	for (uint16_t s : { sym::code::Init, sym::code::Title, static_cast<uint16_t>(0x8aee),
			sym::code::NewGame, sym::code::MainLoop, static_cast<uint16_t>(0x89c6),
			static_cast<uint16_t>(0x8a23), static_cast<uint16_t>(0x841a),
			static_cast<uint16_t>(0x841f), static_cast<uint16_t>(0x8424) })
		gSyncs.insert(s);

	// The original runs one segment ahead.
	int refAt = sym::code::Init;
	long segment = 0;
	uint8_t keys[11];
	memset(keys, 0xff, sizeof(keys));
	uint8_t joystick = 0;
	auto chooseInput = [&]() {
		memset(keys, 0xff, sizeof(keys));
		joystick = 0;
		int r = rand() % 100;
		if (r < 70) {
			static const uint8_t kDirs[] = { 1, 2, 4, 8, 0 };
			joystick = kDirs[rand() % 5];
		}
		if (rand() % 8 == 0)
			joystick |= 0x10;
		if (rand() % 400 == 0)
			keys[4] &= static_cast<uint8_t>(~0x20);	// P
		if (escape && rand() % 3000 == 0)
			keys[7] &= static_cast<uint8_t>(~0x04);	// ESC
		for (int i = 0; i < 11; i++)
			gRef.m.SetKey(i, 0, false);
	};
	auto applyInput = [&]() {
		for (int row = 0; row < 11; row++)
			for (int bit = 0; bit < 8; bit++)
				gRef.m.SetKey(row, bit, (keys[row] & (1 << bit)) == 0);
		gRef.m.SetJoystick(joystick);
		memcpy(gKeyMatrix, keys, sizeof(keys));
		gJoystick[0] = joystick;
		gJoystick[1] = 0;
	};
	auto advanceRef = [&]() {
		int pc = gRef.RunToSync(gSyncs, 400ull * Machine1::kCyclesPerFrame * 60);
		while (pc == -2) {
			gRef.m.Vdp().StartVBlank();
			gRef.Interrupt();
			pc = gRef.RunToSync(gSyncs, 400ull * Machine1::kCyclesPerFrame * 60);
		}
		return pc;
	};

	chooseInput();
	applyInput();
	refAt = sym::code::Init;
	gRefreshValue = []() -> uint8_t {
		if (gRef.refresh.empty())
			return 0;
		uint8_t v = gRef.refresh.front();
		gRef.refresh.pop_front();
		return v;
	};

	gHost.sync = [&](uint16_t address) {
		if (!gSyncs.count(address))
			return;
		char what[96];
		snprintf(what, sizeof(what), "segment %ld at %s (original at %s)", segment,
			SyncName(address), refAt >= 0 ? SyncName(static_cast<uint16_t>(refAt)) : "-");
		static uint64_t refStart = 0, nativeStart = 0;
		static uint16_t lastAddress = 0;
		if (timing && lastAddress == sym::code::MainLoop && address == sym::code::MainLoop) {
			printf("T %d %llu %llu\n", gMem[sym::Level],
				static_cast<unsigned long long>(gRef.time - refStart - gRef.handlerTime),
				static_cast<unsigned long long>(gClock.cycles - nativeStart));
		}
		lastAddress = address;
		if (refAt != address) {
			printf("FAIL %s: the two sides took different paths\n", what);
			gFailures++;
			throw Done();
		}
		gSyncCount[address == sym::code::Init ? 0 : address == sym::code::Title ? 1
			: address == 0x8aee ? 2 : address == sym::code::NewGame ? 3
			: address == 0x89c6 ? 5 : 4]++;
		if (warp > 0 && address == sym::code::MainLoop && gSyncCount[4] % warp == 0
			&& gMem[sym::DotsEaten] < 0xcc && gMem[sym::DemoMode] == 0) {
			gMem[sym::DotsEaten] = 0xcc;
			gRef.m.Ram()[sym::DotsEaten] = 0xcc;
		}
		if (poke > 0 && address == sym::code::MainLoop && rand() % poke == 0
			&& gMem[sym::Dying] == 0) {
			auto set = [&](uint16_t a, uint8_t v) {
				gMem[a] = v;
				gRef.m.Ram()[a] = v;
			};
			int what = rand() % 4;
			if (what == 0)
				set(sym::FruitTimer + 1, 0xff);
			else if (what == 1 && gMem[sym::FruitShown] != 0) {
				set(sym::Player, gMem[sym::FruitSprite]);
				set(sym::PlayerX, gMem[sym::FruitSprite + 1]);
			} else if (what == 2) {
				// a pill of the maze map
				std::vector<int> pills;
				for (int i = 0; i < 0x300; i++) {
					if (gMem[sym::MazeMap + i] == 0x01)
						pills.push_back(i);
				}
				if (!pills.empty()) {
					int cell = pills[rand() % pills.size()];
					int row = cell / 32, col = cell % 32;
					set(sym::Player, static_cast<uint8_t>(8 * (row - 1) + 3));
					set(sym::PlayerX, static_cast<uint8_t>(8 * (col - 1) + 3));
				}
			} else if (what == 3) {
				int g = rand() % 4;
				if (gMem[sym::GhostInHouse + g] == 0) {
					set(sym::Ghosts + g * 4, gMem[sym::Player]);
					set(sym::Ghosts + g * 4 + 1, gMem[sym::PlayerX]);
				}
			}
		}
		if (!Compare(what))
			CopyToNative();
		if (!gRef.refresh.empty() && !gQuiet)
			printf("segment %ld: %zu LD A,R values left over\n", segment, gRef.refresh.size());
		gRef.refresh.clear();
		// interrupts of the frames that passed
		int frames = gRef.frames;
		gRef.frames = 0;
		for (int i = 0; i < frames; i++) {
			bool refEnabled = gRef.m.Cpu().r.iff1;
			if (refEnabled) {
				gRef.m.Vdp().StartVBlank();
				gRef.Interrupt();
			}
			if (gClock.interruptsEnabled) {
				gClock.interruptPending = true;
				gInInterrupt = true;
				RunInterrupt();
				gInInterrupt = false;
			}
		}
		if (frames > 0) {
			snprintf(what, sizeof(what), "segment %ld after %d interrupts", segment, frames);
			if (!Compare(what))
				CopyToNative();
		}
		if (++segment >= segments)
			throw Done();
		if (segment % 20 == 0 || address == 0x89c6 || address == 0x8a23
			|| (address >= 0x841a && address <= 0x8424))
			chooseInput();
		applyInput();
		refStart = gRef.time;
		gRef.handlerTime = 0;
		nativeStart = gClock.cycles;
		refAt = advanceRef();
		if (refAt < 0) {
			printf("segment %ld: the original did not reach a sync point\n", segment);
			throw Done();
		}
	};

	if (profile) {
		gRef.profile = true;
		gCostHook = [](uint32_t cycles, uint16_t routine) {
			if (gInInterrupt)
				return;
			nativeCost[routine] += cycles;
			nativeCalls[routine]++;
		};
	}
	try {
		Run(sym::code::Init);
	} catch (const Done&) {
	}
	printf("sync points: Init %ld, Title %ld, title loop %ld, NewGame %ld, main loop %ld,"
		" name entry %ld\n", gSyncCount[0], gSyncCount[1], gSyncCount[2], gSyncCount[3],
		gSyncCount[4], gSyncCount[5]);
	printf("level %d, score digits", gMem[sym::Level]);
	for (int i = 0; i < 6; i++)
		printf(" %d", gMem[sym::Score + i]);
	printf(", lives %d, demo %d\n", gMem[sym::Lives], gMem[sym::DemoMode]);
	if (profile) {
		std::vector<std::pair<long long, uint16_t> > rows;
		std::set<uint16_t> all;
		for (auto& e : gRef.exclusive)
			all.insert(e.first);
		for (auto& e : nativeCost)
			all.insert(e.first);
		for (uint16_t a : all) {
			long long d = static_cast<long long>(nativeCost[a])
				- static_cast<long long>(gRef.exclusive[a]);
			rows.push_back(std::make_pair(d < 0 ? -d : d, a));
		}
		std::sort(rows.begin(), rows.end());
		std::reverse(rows.begin(), rows.end());
		for (auto& e : gRef.exits) {
			if (e.second.second >= 20 && e.second.first / e.second.second < 20000)
				printf("X %04X %04X %llu %.1f\n", e.first >> 16, e.first & 0xffff,
					static_cast<unsigned long long>(e.second.second),
					static_cast<double>(e.second.first) / e.second.second);
		}
		printf("routine  ref cycles (calls)  port cycles (calls)\n");
		for (size_t i = 0; i < rows.size() && i < 40; i++) {
			uint16_t a = rows[i].second;
			printf("P %04X %llu (%llu) %llu (%llu)\n", a,
				static_cast<unsigned long long>(gRef.exclusive[a]),
				static_cast<unsigned long long>(gRef.calls[a]),
				static_cast<unsigned long long>(nativeCost[a]),
				static_cast<unsigned long long>(nativeCalls[a]));
		}
	}
	if (timing && gRef.interrupts > 0)
		printf("interrupt handler: %llu cycles on average (%ld)\n",
			static_cast<unsigned long long>(gRef.handlerTotal / gRef.interrupts), gRef.interrupts);
	printf("%ld checks, %ld failures\n", gChecks, gFailures);
	return gFailures == 0 ? 0 : 1;
}
