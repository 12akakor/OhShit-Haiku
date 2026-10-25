/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The rewritten program's machine, see Machine.h.
#include "Machine.h"

#include <string.h>


uint8_t gMem[0x10000];
Tms9918 gVdp;
uint8_t gKeyMatrix[11] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff };
uint8_t gJoystick[2];
uint8_t gPage1[0x900];
Clock gClock;
Host gHost;
void (*gCostHook)(uint32_t, uint16_t) = nullptr;


namespace {

uint8_t sPsgLatch;
uint8_t sPsg[16] = { 0, 0, 0, 0, 0, 0, 0, 0xff };

}	// namespace


void
VdpOut(int port, uint8_t value)
{
	gVdp.WritePort(port, value);
}


uint8_t
VdpIn(int port)
{
	uint8_t value = gVdp.ReadPort(port);
	if (port & 1)
		gClock.interruptPending = false;	// reading S#0 clears it
	return value;
}


void
PsgSelect(uint8_t reg)
{
	sPsgLatch = reg & 0x0f;
}


void
PsgData(uint8_t value)
{
	sPsg[sPsgLatch] = value;
	if (gHost.psgWrite)
		gHost.psgWrite(sPsgLatch, value, gClock.cycles);
}


uint8_t
PsgReg(int reg)
{
	reg &= 0x0f;
	if (reg == 14) {
		// joystick port selected by register 15, bit 6
		uint8_t stick = (sPsg[15] & 0x40) ? gJoystick[1] : gJoystick[0];
		return static_cast<uint8_t>((0x3f & ~stick) | 0x40);
	}
	return sPsg[reg];
}


uint8_t*
PsgRegisters()
{
	return sPsg;
}


void
Cycles(uint32_t cycles)
{
	static bool inside = false;
	gClock.cycles += cycles;
	if (inside)
		return;	// charged from the interrupt: the outer call goes on
	// cleared again also when the host leaves with an exception (quit)
	struct Guard {
		Guard() { inside = true; }
		~Guard() { inside = false; }
	} guard;
	while (gClock.cycles >= gClock.nextFrame) {
		gClock.nextFrame += kCyclesPerFrame;
		gClock.interruptPending = true;
		if (gHost.frame)
			gHost.frame();
		if (gClock.interruptsEnabled && gClock.deliverInterrupts)
			RunInterrupt();
	}
}


void
EI()
{
	gClock.interruptsEnabled = true;
	if (gClock.interruptPending && gClock.deliverInterrupts)
		RunInterrupt();
}


void
DI()
{
	gClock.interruptsEnabled = false;
}
