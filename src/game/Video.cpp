/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The program's own video routines (#9F3C-#A04E) and busy waits.
#include "Game.h"
#include "Timing.h"

#include "Bios.h"


// WithDi (#A03B) runs the rest of the routine with interrupts off and then
// enables them again, unless the interrupt routine (or an intermission) has
// set InInterrupt.
namespace {

// WithDi (#A03B) keeps the address of the routine body in its own CALL.
inline void
WithDi(uint16_t body)
{
	DI();
	SetM16(0xa042, body);
}

}	// namespace


void
WithDiEnd()
{
	if (M(sym::InInterrupt) == 0)
		EI();
}


void
VdpReadAddress(uint16_t hl)
{
	VdpOut(1, static_cast<uint8_t>(hl));
	VdpOut(1, static_cast<uint8_t>((hl >> 8) & 0x3f));
}


void
VdpWriteAddress(uint16_t hl)
{
	VdpOut(1, static_cast<uint8_t>(hl));
	VdpOut(1, static_cast<uint8_t>(((hl >> 8) & 0x3f) | 0x40));
}


void
VdpFillEi(uint16_t hl, uint16_t bc, uint8_t a)
{
	VdpWriteAddress(hl);
	uint32_t count = bc == 0 ? 0x10000 : bc;
	for (uint32_t i = 0; i < count; i++)
		VdpOut(0, a);
}


void
VdpFill(uint16_t hl, uint16_t bc, uint8_t a)
{
	Cost(timing::VdpFill(bc == 0 ? 0x10000 : bc), 0x9f65);
	WithDi(0x9f68);
	VdpFillEi(hl, bc, a);
	WithDiEnd();
}


void
VdpPoke(uint16_t hl, uint8_t a)
{
	Cost(timing::VDP_POKE, 0x9f81);
	WithDi(0x9f84);
	VdpWriteAddress(hl);
	VdpOut(0, a);
	WithDiEnd();
}


uint8_t
VdpPeek(uint16_t hl)
{
	Cost(timing::VDP_PEEK, 0x9f92);
	WithDi(0x9f95);
	VdpReadAddress(hl);
	uint8_t a = VdpIn(0);
	WithDiEnd();
	return a;
}


void
VdpWriteReg(uint8_t reg, uint8_t value)
{
	VdpOut(1, value);
	VdpOut(1, static_cast<uint8_t>(reg | 0x80));
	M(static_cast<uint16_t>(bios::RG0SAV + reg)) = value;
}


void
SetColours()
{
	Cost(timing::SET_COLOURS, 0x9fa1);
	WithDi(0x9fa4);
	uint8_t mode = M(bios::SCRMOD);
	if (mode == 0) {
		VdpWriteReg(7, static_cast<uint8_t>((M(bios::FORCLR) << 4) | M(bios::BAKCLR)));
	} else {
		VdpWriteReg(7, M(bios::BDRCLR));
		if (mode == 1) {
			uint8_t colour = static_cast<uint8_t>((M(bios::FORCLR) << 4)
				| M(bios::BAKCLR));
			VdpFillEi(M16(bios::T32COL), 0x20, colour);
		}
	}
	WithDiEnd();
}


void
VdpRead(uint16_t vram, uint16_t ram, uint16_t count)
{
	Cost(timing::VdpRead(count == 0 ? 0x10000 : count), 0x9ffd);
	WithDi(0xa000);
	VdpReadAddress(vram);
	uint32_t n = count == 0 ? 0x10000 : count;
	for (uint32_t i = 0; i < n; i++)
		M(static_cast<uint16_t>(ram + i)) = VdpIn(0);
	WithDiEnd();
}


void
VdpWrite(uint16_t ram, uint16_t vram, uint16_t count)
{
	Cost(timing::VdpWrite(count == 0 ? 0x10000 : count), 0xa01b);
	WithDi(0xa01e);
	VdpWriteAddress(vram);
	uint32_t n = count == 0 ? 0x10000 : count;
	for (uint32_t i = 0; i < n; i++)
		VdpOut(0, M(static_cast<uint16_t>(ram + i)));
	WithDiEnd();
}


// ld hl,start / inc hl / ld a,h / or l / jr nz: 11 + 30 per count - 5.
void
WaitCountUp(uint16_t start)
{
	uint32_t n = start == 0 ? 0x10000 : 0x10000 - start;
	Cycles(11 + 30 * n - 5);
}


// ld de,count / dec de / ld a,d / or e / jr nz
void
WaitCountDownDE(uint16_t count)
{
	uint32_t n = count == 0 ? 0x10000 : count;
	Cycles(11 + 30 * n - 5);
}


void
WaitCountDownBC(uint16_t count)
{
	WaitCountDownDE(count);
}
