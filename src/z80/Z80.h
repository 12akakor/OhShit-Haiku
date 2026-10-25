/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_Z80_H
#define OS_Z80_H


// Z80 interpreter with cycle counting. Reference implementation for the
// Oh Shit! port: runs the original program, and is the yardstick for the
// C++ rewrite (differential tests). Not part of the game.
//
// Covers the documented and the commonly relied-on undocumented behaviour
// (X/Y flags, MEMPTR, IXH/IXL, SLL, DD CB/FD CB register copies), checked
// with zexdoc and zexall.

#include <stdint.h>


class Z80Bus {
public:
	virtual					~Z80Bus() {}
	virtual	uint8_t			Read(uint16_t address) = 0;
	virtual	void			Write(uint16_t address, uint8_t value) = 0;
	virtual	uint8_t			In(uint16_t port) = 0;
	virtual	void			Out(uint16_t port, uint8_t value) = 0;
};


enum {
	Z80_CF = 0x01,
	Z80_NF = 0x02,
	Z80_PF = 0x04,
	Z80_XF = 0x08,
	Z80_HF = 0x10,
	Z80_YF = 0x20,
	Z80_ZF = 0x40,
	Z80_SF = 0x80
};


struct Z80Regs {
	union { struct { uint8_t f, a; }; uint16_t af; };
	union { struct { uint8_t c, b; }; uint16_t bc; };
	union { struct { uint8_t e, d; }; uint16_t de; };
	union { struct { uint8_t l, h; }; uint16_t hl; };
	union { struct { uint8_t ixl, ixh; }; uint16_t ix; };
	union { struct { uint8_t iyl, iyh; }; uint16_t iy; };
	uint16_t	af2, bc2, de2, hl2;
	uint16_t	sp, pc, wz;
	uint8_t		i, r;
	bool		iff1, iff2;
	uint8_t		im;
	bool		halted;
};


class Z80 {
public:
							Z80(Z80Bus* bus);

			void			Reset();

	// Executes one instruction (or one HALT cycle); returns T-states.
			int				Step();

	// Maskable interrupt request (IM 1 only is needed here; IM 2 uses
	// the data bus value 0xFF). Returns T-states, 0 if not accepted.
			int				Interrupt();

			Z80Regs			r;
	// Set after EI: interrupts are not accepted before the next
	// instruction.
			bool			eiPending;
	// Extra T-states per M1 cycle (opcode fetch). The MSX inserts one
	// wait state in every M1 cycle.
			int				m1Wait;

private:
			uint8_t			_Fetch();
			uint16_t		_Fetch16();
			uint8_t			_Read(uint16_t a) { return fBus->Read(a); }
			void			_Write(uint16_t a, uint8_t v) { fBus->Write(a, v); }
			uint16_t		_Read16(uint16_t a);
			void			_Write16(uint16_t a, uint16_t v);
			void			_Push(uint16_t v);
			uint16_t		_Pop();

			uint8_t&		_Reg8(int index, int prefix);
			uint16_t&		_Reg16(int index, int prefix);
			uint16_t		_IndexAddress(int prefix);

			int				_StepInner();
			int				_ExecMain(uint8_t op, int prefix);
			int				_ExecCB(int prefix);
			int				_ExecED();

			uint8_t			_Inc(uint8_t v);
			uint8_t			_Dec(uint8_t v);
			void			_Alu(int op, uint8_t v);
			uint8_t			_Rot(int op, uint8_t v);
			void			_Add16(uint16_t& d, uint16_t v);
			void			_Adc16(uint16_t v);
			void			_Sbc16(uint16_t v);
			bool			_Cond(int c);

			Z80Bus*			fBus;
};


#endif	// OS_Z80_H
