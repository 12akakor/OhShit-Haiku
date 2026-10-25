/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// MSX1 test machine, see Machine1.h.
#include "Machine1.h"

#include <string.h>


Machine1::Machine1()
	:
	fCpu(this)
{
	Reset();
}


bool
Machine1::LoadBios(const std::vector<uint8_t>& main,
	const std::vector<uint8_t>& logo)
{
	if (main.size() != 0x8000)
		return false;
	fBios = main;
	fLogo = logo;
	return true;
}


bool
Machine1::LoadCartridge(const std::vector<uint8_t>& rom)
{
	if (rom.empty() || rom.size() > 0x8000)
		return false;
	fCart = rom;
	if (fCart.size() <= 0x4000) {
		// 16 KB or less: mirrored in #8000-#BFFF
		fCart.resize(0x4000, 0xff);
		fCart.insert(fCart.end(), fCart.begin(), fCart.end());
	} else
		fCart.resize(0x8000, 0xff);
	return true;
}


void
Machine1::Reset()
{
	// RAM after power on: not zero on real machines, but C-BIOS clears
	// what it uses; zeros keep the runs reproducible.
	memset(fRam, 0, sizeof(fRam));
	memset(fKeys, 0xff, sizeof(fKeys));
	fSlots = 0;
	fPortC = 0;
	fJoystick = 0;
	fPsgLatch = 0;
	fFrames = 0;
	fCycleDebt = 0;
	fTime = 0;
	fLineStart = 0;
	fCycles = 0;
	psgLog.clear();
	fVdp.Reset();
	fPsg.Reset();
	fCpu.Reset();
	fCpu.m1Wait = 1;
}


uint8_t
Machine1::Read(uint16_t address)
{
	switch (_Slot(address)) {
		case 0:
			if (address < 0x8000)
				return fBios.empty() ? 0xff : fBios[address];
			if (address < 0xc000 && !fLogo.empty()
				&& address - 0x8000u < fLogo.size())
				return fLogo[address - 0x8000];
			return 0xff;
		case 1:
			if (address >= 0x4000 && address < 0xc000 && !fCart.empty())
				return fCart[address - 0x4000];
			return 0xff;
		case 3:
			return fRam[address];
		default:
			return 0xff;
	}
}


void
Machine1::Write(uint16_t address, uint8_t value)
{
	if (_Slot(address) == 3)
		fRam[address] = value;
}


uint8_t
Machine1::In(uint16_t port)
{
	port &= 0xff;
	uint8_t value = 0xff;
	if (port == 0x98 || port == 0x99)
		value = fVdp.ReadPort(port);
	else if (port == 0xa2) {
		if (fPsgLatch == 14) {
			uint8_t stick = (fPsg.Register(15) & 0x40) ? 0 : fJoystick;
			value = static_cast<uint8_t>((0x3f & ~stick) | 0x40);
		} else
			value = fPsg.Register(fPsgLatch);
	} else if (port == 0xa8)
		value = fSlots;
	else if (port == 0xa9) {
		int row = fPortC & 0x0f;
		value = row < 11 ? fKeys[row] : 0xff;
	} else if (port == 0xaa)
		value = fPortC;
	if (onPortIn)
		onPortIn(port, value);
	return value;
}


void
Machine1::Out(uint16_t port, uint8_t value)
{
	port &= 0xff;
	if (onPortOut)
		onPortOut(port, value);
	switch (port) {
		case 0x98:
		case 0x99:
			fVdp.WritePort(port, value);
			break;
		case 0xa0:
			fPsgLatch = value & 0x0f;
			break;
		case 0xa1:
			if (logPsg) {
				PsgWrite w = { fLineStart + static_cast<uint64_t>(fCycles),
					fPsgLatch, value };
				psgLog.push_back(w);
			}
			fPsg.Write(fPsgLatch, value);
			break;
		case 0xa8:
			fSlots = value;
			break;
		case 0xaa:
			fPortC = value;
			break;
		case 0xab:
			// bit set/reset of port C
			if ((value & 0x80) == 0) {
				int bit = (value >> 1) & 7;
				if (value & 1)
					fPortC |= static_cast<uint8_t>(1 << bit);
				else
					fPortC &= static_cast<uint8_t>(~(1 << bit));
			}
			break;
	}
}


void
Machine1::SetKey(int row, int bit, bool down)
{
	if (row < 0 || row > 10 || bit < 0 || bit > 7)
		return;
	if (down)
		fKeys[row] &= static_cast<uint8_t>(~(1 << bit));
	else
		fKeys[row] |= static_cast<uint8_t>(1 << bit);
}


int
Machine1::StepInstruction()
{
	return fCpu.Step();
}


void
Machine1::RunFrame()
{
	for (int line = 0; line < Tms9918::kLinesPerFrame; line++) {
		if (line == Tms9918::kLines)
			fVdp.StartVBlank();
		fLineStart = fTime;
		fCycles = fCycleDebt;
		while (fCycles < kCyclesPerLine) {
			if (fVdp.InterruptPending() && fCpu.r.iff1 && !fCpu.eiPending) {
				int c = fCpu.Interrupt();
				if (c > 0) {
					fCycles += c;
					continue;
				}
			}
			if (onStep)
				onStep();
			fCycles += fCpu.Step();
		}
		fCycleDebt = fCycles - kCyclesPerLine;
		fTime = fLineStart + kCyclesPerLine;
		if (line < Tms9918::kLines)
			fVdp.RenderLine(line);
	}
	fFrames++;
}
