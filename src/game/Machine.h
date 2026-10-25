/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_MACHINE_H
#define OS_MACHINE_H


// What the rewritten program works on: the same things the original used,
// kept in the same form so every routine can be compared with the original.
//
// - gMem: the 64 KB of RAM. The program image (#8107-#EFFF) comes from the
//   player's game file; the variables live at the addresses of the
//   original (see Symbols.h).
// - gVdp: the video chip, driven through its ports as the original does.
// - the PSG registers, the keyboard matrix and the joysticks.
// - the clock: CPU cycles of the original. Busy waits and work charge
//   cycles; every 59736 cycles a video frame ends, and the interrupt runs
//   as soon as interrupts are enabled.

#include <stdint.h>

#include <functional>

#include "../video/Tms9918.h"


extern uint8_t gMem[0x10000];
extern Tms9918 gVdp;

static inline uint8_t&
M(uint16_t address)
{
	return gMem[address];
}

static inline uint16_t
M16(uint16_t address)
{
	return static_cast<uint16_t>(gMem[address]
		| (gMem[static_cast<uint16_t>(address + 1)] << 8));
}

static inline void
SetM16(uint16_t address, uint16_t value)
{
	gMem[address] = static_cast<uint8_t>(value);
	gMem[static_cast<uint16_t>(address + 1)] = static_cast<uint8_t>(value >> 8);
}


// Ports of the VDP (the original takes them from the BIOS, #98 and #99).
void		VdpOut(int port, uint8_t value);
uint8_t		VdpIn(int port);

// PSG through the ports #A0/#A1 (the speech) and the BIOS (all else).
void		PsgSelect(uint8_t reg);
void		PsgData(uint8_t value);
uint8_t		PsgReg(int reg);
// The 16 registers (for the tests and the sound output).
uint8_t*	PsgRegisters();

// Input: keyboard matrix (0 bits pressed) and joysticks 1 and 2
// (bits 0..5 = up, down, left, right, A, B; 1 = pressed).
extern uint8_t gKeyMatrix[11];
extern uint8_t gJoystick[2];
// Page 1 at #5000-#58FF (BIOS or cartridge), read by the demo's random
// numbers. The port uses what the ROM version has there: the program
// image from #9000 on.
extern uint8_t gPage1[0x900];


// The clock.
struct Clock {
	uint64_t	cycles = 0;			// CPU cycles since the start
	uint64_t	nextFrame = 59736;	// end of the current video frame
	bool		interruptsEnabled = false;
	bool		interruptPending = false;	// VDP status bit 7
	// false in the differential tests: interrupts run only where the
	// tests deliver them (RunInterrupt).
	bool		deliverInterrupts = true;
};
extern Clock gClock;

enum { kCyclesPerFrame = 59736 };

// Charges CPU time; ends frames and runs the interrupt when due.
void		Cycles(uint32_t cycles);
void		EI();
void		DI();

// Hooks of the host (window, sound) and of the tests.
struct Host {
	// End of a video frame: show it, read the input, keep real time.
	std::function<void()>	frame;
	// Every PSG register write, with the clock (for the sound output).
	std::function<void(int, uint8_t, uint64_t)>	psgWrite;
	// The original's code at 'address' is reached (tests keep the two
	// sides in step here). Not called when empty.
	std::function<void(uint16_t)>	sync;
};
extern Host gHost;

static inline void
Sync(uint16_t address)
{
	if (gHost.sync)
		gHost.sync(address);
}

// Restarts the program from 'address' with an empty stack (the original's
// "ld sp,(SavedSP): jp ..."). Implemented with an exception caught by the
// main routine.
struct Restart {
	uint16_t	address;
};

// The interrupt handler that runs on a frame end: the BIOS one or the
// game's own (H.KEYI).
void		RunInterrupt();


#endif	// OS_MACHINE_H
