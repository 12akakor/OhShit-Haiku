/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// BIOS routines, see Bios.h. Written after the behaviour of C-BIOS 0.29
// (MSX1 machine): register writes, VRAM contents, system variables and the
// interrupt enable state the caller gets back.
#include "Bios.h"

#include "Machine.h"
#include "Timing.h"


namespace bios {

namespace {

void
SetWrite(uint16_t address)
{
	VdpOut(1, static_cast<uint8_t>(address));
	VdpOut(1, static_cast<uint8_t>(((address >> 8) & 0x3f) | 0x40));
}


void
Fill(uint16_t address, uint8_t value, uint16_t count)
{
	SetWrite(address);
	for (uint32_t i = 0; i < count; i++)
		VdpOut(0, value);
}


void
SetBaseRegisters(uint16_t table, bool text)
{
	// Shifts of the name, colour, pattern, attribute and sprite pattern
	// table addresses into registers 2 to 6.
	static const int kShift[5] = { 10, 6, 11, 7, 11 };
	for (int i = 0; i < 5; i++) {
		if (text && (i == 1 || i >= 3))
			continue;
		uint16_t address = M16(static_cast<uint16_t>(table + i * 2));
		WRTVDP(static_cast<uint8_t>(2 + i), static_cast<uint8_t>(address >> kShift[i]));
	}
}


void
DisableScreen()
{
	WRTVDP(1, M(RG1SAV) & 0xbf);
}


// Writes R#0-R#7 from RG0SAV-RG7SAV, then enables the screen.
void
FinishModeChange()
{
	for (int i = 0; i < 8; i++) {
		VdpOut(1, M(static_cast<uint16_t>(RG0SAV + i)));
		VdpOut(1, static_cast<uint8_t>(0x80 + i));
	}
	EI();
	WRTVDP(1, M(RG1SAV) | 0x40);
}


void
ClearSpriteAttributes()
{
	SetWrite(M16(ATRBAS));
	uint8_t pattern = 0;
	bool size16 = M(RG1SAV) & 0x02;
	for (int i = 0; i < 32; i++) {
		VdpOut(0, 209);
		VdpOut(0, 0);
		VdpOut(0, pattern);
		pattern = static_cast<uint8_t>(pattern + (size16 ? 4 : 1));
		VdpOut(0, M(FORCLR));
	}
	EI();
}


void
ClearText(uint16_t count)
{
	Fill(M16(NAMBAS), 0x20, count);
	EI();
	M(CSRY) = 1;
	M(CSRX) = 1;
}


uint8_t sScanCount = 3;


void
RawWrtpsg(uint8_t reg, uint8_t value)
{
	DI();
	PsgSelect(reg);
	PsgData(value);
	EI();
}


uint8_t
RawRdpsg(uint8_t reg)
{
	PsgSelect(reg);
	return PsgReg(reg);
}


uint8_t
RawSnsmat(uint8_t row)
{
	DI();
	uint8_t value = row < 11 ? gKeyMatrix[row] : 0xff;
	EI();
	return value;
}

}	// namespace


void
InitSystemVariables()
{
	SetM16(T32NAM, 0x1800);
	SetM16(T32COL, 0x2000);
	SetM16(T32CGP, 0x0000);
	SetM16(T32ATR, 0x1b00);
	SetM16(T32PAT, 0x3800);
	SetM16(TXTNAM, 0x0000);
	SetM16(TXTCGP, 0x0800);
	M(LINL40) = 39;
	M(LINL32) = 29;
	M(FORCLR) = 15;
	M(BAKCLR) = 4;
	M(BDRCLR) = 4;
	M(RG1SAV) = 0xe2;
	M(HKEYI) = 0xc9;
}


void
WRTVDP(uint8_t reg, uint8_t value)
{
	reg &= 0x7f;
	VdpOut(1, value);
	VdpOut(1, static_cast<uint8_t>(reg | 0x80));
	EI();
	if (reg < 8)
		M(static_cast<uint16_t>(RG0SAV + reg)) = value;
}


void
WRTVRM(uint16_t address, uint8_t value)
{
	SetWrite(address);
	EI();
	VdpOut(0, value);
}


void
LDIRVM(uint16_t source, uint16_t vram, uint16_t count)
{
	SetWrite(vram);
	EI();
	uint32_t n = count == 0 ? 0x10000 : count;
	for (uint32_t i = 0; i < n; i++)
		VdpOut(0, M(static_cast<uint16_t>(source + i)));
}


void
CHGCLR()
{
	uint8_t mode = M(SCRMOD);
	WRTVDP(7, static_cast<uint8_t>((M(FORCLR) << 4) | M(BDRCLR)));
	if (mode != 1)
		return;
	Fill(M16(T32COL), static_cast<uint8_t>((M(FORCLR) << 4) | M(BAKCLR)), 0x20);
	EI();
}


void
CLRSPR()
{
	Cost(timing::CLRSPR, 0x0069);
	if (M(SCRMOD) == 0)
		return;
	ClearSpriteAttributes();
	Fill(M16(PATBAS), 0, 256 * 8);
	EI();
}


void
INITXT()
{
	DisableScreen();
	M(SCRMOD) = 0;
	M(OLDSCR) = 0;
	M(LINLEN) = M(LINL40);
	M(CSRY) = 1;
	M(CSRX) = 1;
	SetM16(NAMBAS, M16(TXTNAM));
	SetM16(CGPBAS, M16(TXTCGP));
	CHGCLR();
	WRTVDP(0, M(RG0SAV) & 0xf1);
	WRTVDP(1, static_cast<uint8_t>((M(RG1SAV) & 0xe7) | 0x10));
	SetBaseRegisters(TXTNAM, true);
	ClearText(40 * 24);
	FinishModeChange();
}


void
INIT32()
{
	Cost(timing::INIT32, 0x006f);
	DisableScreen();
	M(SCRMOD) = 1;
	M(OLDSCR) = 1;
	M(CSRY) = 1;
	M(CSRX) = 1;
	CHGCLR();
	SetM16(NAMBAS, M16(T32NAM));
	SetM16(CGPBAS, M16(T32CGP));
	SetM16(PATBAS, M16(T32PAT));
	SetM16(ATRBAS, M16(T32ATR));
	M(LINLEN) = M(LINL32);
	WRTVDP(0, M(RG0SAV) & 0xf1);
	WRTVDP(1, M(RG1SAV) & 0xe7);
	SetBaseRegisters(T32NAM, false);
	ClearSpriteAttributes();
	ClearText(32 * 24);
	FinishModeChange();
}


void
CLS()
{
	if (M(SCRMOD) == 0)
		ClearText(40 * 24);
	else if (M(SCRMOD) == 1)
		ClearText(32 * 24);
}


void
CHPUT(uint8_t c)
{
	// Text output for the joystick test: printable characters, CR, LF and
	// ESC Y row column. No cursor is drawn.
	static int escape = 0;
	static uint8_t escapeRow;
	uint8_t mode = M(SCRMOD);
	if (mode > 1)
		return;
	int width = M(LINLEN);
	if (escape == 1) {
		escape = c == 'Y' ? 2 : 0;
		return;
	}
	if (escape == 2) {
		escapeRow = c;
		escape = 3;
		return;
	}
	if (escape == 3) {
		M(CSRY) = static_cast<uint8_t>(escapeRow - 0x1f);
		M(CSRX) = static_cast<uint8_t>(c - 0x1f);
		escape = 0;
		return;
	}
	if (c == 0x1b) {
		escape = 1;
		return;
	}
	if (c == 0x0d) {
		M(CSRX) = 1;
		return;
	}
	if (c == 0x0a) {
		if (M(CSRY) < 24)
			M(CSRY)++;
		return;
	}
	if (c < 0x20)
		return;
	int columns = mode == 0 ? 40 : 32;
	uint16_t address = static_cast<uint16_t>(M16(NAMBAS)
		+ (M(CSRY) - 1) * columns + (M(CSRX) - 1));
	WRTVRM(address, c);
	if (M(CSRX) < width)
		M(CSRX)++;
	else {
		M(CSRX) = 1;
		if (M(CSRY) < 24)
			M(CSRY)++;
	}
}


void
KILBUF()
{
}


void
WRTPSG(uint8_t reg, uint8_t value)
{
	Cost(timing::WRTPSG, 0x0093);
	RawWrtpsg(reg, value);
}


uint8_t
RDPSG(uint8_t reg)
{
	Cost(timing::RDPSG, 0x0096);
	return RawRdpsg(reg);
}


uint8_t
SNSMAT(uint8_t row)
{
	Cost(timing::SNSMAT, 0x0141);
	return RawSnsmat(row);
}


uint8_t
GTSTCK(uint8_t stick)
{
	static const uint8_t kJoystick[16] = {
		0, 1, 5, 0, 7, 8, 6, 7, 3, 2, 4, 3, 0, 1, 5, 0
	};
	static const uint8_t kKeyboard[16] = {
		0, 7, 1, 8, 5, 6, 0, 7, 3, 0, 2, 1, 4, 5, 3, 0
	};
	if (stick == 0) {
		Cost(timing::GTSTCK_KEYBOARD, 0x00d5);
		uint8_t row = RawSnsmat(8);
		row = static_cast<uint8_t>(((row >> 4) | (row << 4)));
		return kKeyboard[~row & 0x0f];
	}
	Cost(timing::GTSTCK_JOYSTICK, 0x00d5);
	uint8_t select = stick == 1 ? 0x00 : 0x40;
	uint8_t r15 = RawRdpsg(15);
	EI();
	RawWrtpsg(15, static_cast<uint8_t>((r15 & 0xbf) | select));
	uint8_t bits = RawRdpsg(14);
	EI();
	return kJoystick[~bits & 0x0f];
}


uint8_t
GTTRIG(uint8_t trigger)
{
	if (trigger >= 5 || trigger == 0) {
		Cost(timing::GTTRIG_SPACE, 0x00d8);
		return (RawSnsmat(8) & 1) ? 0x00 : 0xff;
	}
	Cost(timing::GTTRIG_JOYSTICK, 0x00d8);
	DI();
	uint8_t which = static_cast<uint8_t>(trigger - 1);
	uint8_t enable = (which & 1) ? 0x4c : 0x03;
	uint8_t r15 = RawRdpsg(15);
	RawWrtpsg(15, static_cast<uint8_t>((r15 & 0xbf) | enable));
	uint8_t mask = (which & 2) ? 0x20 : 0x10;
	DI();
	uint8_t bits = RawRdpsg(14);
	EI();
	return (bits & mask) ? 0x00 : 0xff;
}


void
Interrupt()
{
	Cost(timing::BIOS_INTERRUPT, 0x853b);
	VdpIn(1);
	SetM16(JIFFY, static_cast<uint16_t>(M16(JIFFY) + 1));
	if (--sScanCount == 0) {
		sScanCount = 3;
		for (int row = 0; row < 11; row++)
			M(static_cast<uint16_t>(NEWKEY + row)) = gKeyMatrix[row];
	}
}

}	// namespace bios
