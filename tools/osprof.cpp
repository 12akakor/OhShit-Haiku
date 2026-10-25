/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Profile of the original program: CPU cycles per routine, for the timing
// of the rewrite.
//
//   osprof game.rom frames [-random seed] [-warp n]
//
// Every CALL starts a frame on a shadow stack; the frame ends when the
// stack pointer rises above its level at the call (RET, or the stack
// tricks of the program). Exclusive cycles (without the routines it
// calls) are summed per routine and per exit address (the RET that ended
// it), so different paths through a routine can be told apart. Interrupt
// handlers are left out.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "../src/msx1/Machine1.h"


namespace {

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


struct Frame {
	uint16_t	entry;
	uint16_t	sp;			// SP before the call (frame ends when SP > sp - 2)
	uint64_t	exclusive;
	uint16_t	lastPc;
};

struct Stat {
	uint64_t	calls = 0;
	uint64_t	cycles = 0;
	uint64_t	min = ~0ull;
	uint64_t	max = 0;
};

}	// namespace


int
main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: osprof game.rom frames [-random seed]\n");
		return 1;
	}
	long frames = atol(argv[2]);
	long seed = 1;
	long warp = 0;
	for (int i = 3; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "-random"))
			seed = atol(argv[i + 1]);
		else if (!strcmp(argv[i], "-warp"))
			warp = atol(argv[i + 1]);
	}

	static Machine1 m;
	if (!m.LoadBios(ReadFile("/usr/share/cbios/cbios_main_msx1.rom"),
			ReadFile("/usr/share/cbios/cbios_logo_msx1.rom"))
		|| !m.LoadCartridge(ReadFile(argv[1])))
		return 1;
	m.Reset();

	std::vector<Frame> stack;
	std::map<uint16_t, Stat> perRoutine;
	std::map<uint32_t, Stat> perExit;
	uint64_t lastTime = 0;
	bool inInterrupt = false;
	uint16_t interruptSp = 0;
	bool active = false;

	auto close = [&](const Frame& f) {
		Stat& s = perRoutine[f.entry];
		s.calls++;
		s.cycles += f.exclusive;
		if (f.exclusive < s.min)
			s.min = f.exclusive;
		if (f.exclusive > s.max)
			s.max = f.exclusive;
		Stat& e = perExit[(static_cast<uint32_t>(f.entry) << 16) | f.lastPc];
		e.calls++;
		e.cycles += f.exclusive;
		if (f.exclusive < e.min)
			e.min = f.exclusive;
		if (f.exclusive > e.max)
			e.max = f.exclusive;
	};

	uint16_t prevPc = 0, prevSp = 0;
	m.onStep = [&]() {
		Z80Regs& r = m.Cpu().r;
		uint64_t now = m.Now();
		// cycles of the previous instruction go to the frame it ran in
		uint64_t spent = now >= lastTime ? now - lastTime : 0;
		lastTime = now;
		if (active && !inInterrupt && !stack.empty()) {
			stack.back().exclusive += spent;
			stack.back().lastPc = prevPc;
		}
		uint16_t pc = r.pc;
		if (pc == 0x8107)
			active = true;
		if (!active) {
			prevPc = pc;
			prevSp = r.sp;
			return;
		}
		// interrupt entry: PC jumped to #0038
		if (pc == 0x0038 && !inInterrupt) {
			inInterrupt = true;
			interruptSp = static_cast<uint16_t>(r.sp + 2);
		}
		if (inInterrupt) {
			if (r.sp >= interruptSp && pc != 0x0038)
				inInterrupt = false;
			prevPc = pc;
			prevSp = r.sp;
			return;
		}
		// a call happened: SP went down by 2 and the return address is
		// the instruction after the previous one
		uint8_t op = m.Read(prevPc);
		bool isCall = op == 0xcd || (op & 0xc7) == 0xc4 || (op & 0xc7) == 0xc7;
		if (isCall && r.sp == static_cast<uint16_t>(prevSp - 2)) {
			Frame f = { pc, prevSp, 0, pc };
			stack.push_back(f);
		}
		while (!stack.empty() && r.sp >= stack.back().sp) {
			close(stack.back());
			stack.pop_back();
		}
		prevPc = pc;
		prevSp = r.sp;
	};

	// cycles: add the cost of each instruction to the top frame
	uint64_t total = 0;
	srand(static_cast<unsigned>(seed));
	uint8_t joy = 0;
	for (long f = 0; f < frames; f++) {
		if (rand() % 20 == 0) {
			static const uint8_t kDirs[] = { 1, 2, 4, 8, 0 };
			joy = kDirs[rand() % 5];
		}
		uint8_t j = joy;
		if (rand() % 60 == 0)
			j |= 0x10;
		m.SetJoystick(j);
		if (warp > 0 && f % warp == 0 && m.Ram()[0x9b26] < 0xcc && m.Ram()[0x8119] == 0)
			m.Ram()[0x9b26] = 0xcc;
		// step the frame by hand to attribute cycles
		uint64_t before = m.Time();
		(void)before;
		m.RunFrame();
	}
	(void)total;
	(void)lastTime;

	printf("routine calls avg min max\n");
	for (auto& p : perRoutine) {
		printf("R %04X %llu %.1f %llu %llu\n", p.first,
			static_cast<unsigned long long>(p.second.calls),
			static_cast<double>(p.second.cycles) / p.second.calls,
			static_cast<unsigned long long>(p.second.min),
			static_cast<unsigned long long>(p.second.max));
	}
	for (auto& p : perExit) {
		printf("E %04X %04X %llu %.1f %llu %llu\n", p.first >> 16, p.first & 0xffff,
			static_cast<unsigned long long>(p.second.calls),
			static_cast<double>(p.second.cycles) / p.second.calls,
			static_cast<unsigned long long>(p.second.min),
			static_cast<unsigned long long>(p.second.max));
	}
	return 0;
}
