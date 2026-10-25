/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_MACHINE1_H
#define OS_MACHINE1_H


// An MSX1 for the tests, laid out like the C-BIOS MSX1 machine of openMSX:
// slot 0  the BIOS (32 KB at #0000) and an optional logo ROM at #8000
// slot 1  the cartridge (up to 32 KB at #4000)
// slot 3  64 KB RAM
// plus the PPI (slots, keyboard), the TMS9918A and the PSG. The BIOS is
// real Z80 code (C-BIOS, from the system), nothing is emulated in C++.

#include <functional>
#include <stdint.h>
#include <vector>

#include "../z80/Z80.h"
#include "Psg.h"
#include "Tms9918.h"


class Machine1 : public Z80Bus {
public:
	enum {
		kCyclesPerLine = 228,
		kCyclesPerFrame = kCyclesPerLine * Tms9918::kLinesPerFrame
	};

	struct PsgWrite {
		uint64_t	time;	// CPU cycle
		uint8_t		reg;
		uint8_t		value;
		uint16_t	pc;		// of the instruction after the OUT
	};

							Machine1();

			bool			LoadBios(const std::vector<uint8_t>& main,
								const std::vector<uint8_t>& logo);
			bool			LoadCartridge(const std::vector<uint8_t>& rom);
			void			Reset();

	// Runs one video frame (262 lines).
			void			RunFrame();
	// One instruction, without interrupts.
			int				StepInstruction();

	// Keyboard matrix: row 0..10, bit 0..7; true = pressed.
			void			SetKey(int row, int bit, bool down);
	// Joystick port 1: bits 0..5 = up, down, left, right, A, B.
			void			SetJoystick(uint8_t bits) { fJoystick = bits; }

			Tms9918&		Vdp() { return fVdp; }
			Psg&			GetPsg() { return fPsg; }
			Z80&			Cpu() { return fCpu; }
	// The 64 KB RAM of slot 3.
			uint8_t*		Ram() { return fRam; }
			uint8_t			SlotRegister() const { return fSlots; }
			uint64_t		Frames() const { return fFrames; }
			uint64_t		Time() const { return fTime; }

	// Z80Bus
	virtual	uint8_t			Read(uint16_t address);
	virtual	void			Write(uint16_t address, uint8_t value);
	virtual	uint8_t			In(uint16_t port);
	virtual	void			Out(uint16_t port, uint8_t value);

	// All PSG writes since the last clear, with their time (for the speech).
			std::vector<PsgWrite> psgLog;
			bool			logPsg = false;
	// Test hooks.
			std::function<void()>	onStep;
			std::function<void(uint16_t, uint8_t)>	onPortOut;
			std::function<void(uint16_t, uint8_t)>	onPortIn;

private:
			int				_Slot(uint16_t address) const
								{ return (fSlots >> ((address >> 14) * 2)) & 3; }

			Z80				fCpu;
			Tms9918			fVdp;
			Psg				fPsg;
			std::vector<uint8_t> fBios;
			std::vector<uint8_t> fLogo;
			std::vector<uint8_t> fCart;
			uint8_t			fRam[0x10000];
			uint8_t			fSlots;
			uint8_t			fPortC;
			uint8_t			fKeys[11];	// 0 bits = pressed
			uint8_t			fJoystick;
			uint8_t			fPsgLatch;
			uint64_t		fFrames;
			int				fCycleDebt;
			uint64_t		fTime;
			uint64_t		fLineStart;
			int				fCycles;
};


#endif	// OS_MACHINE1_H
