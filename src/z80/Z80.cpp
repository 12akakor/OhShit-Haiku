/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Z80 interpreter, see z80.h.
//
// Decoding follows the x/y/z/p/q scheme of the opcode tables
// (op = xx yyy zzz, p = y >> 1, q = y & 1). Flag and MEMPTR behaviour
// follow "The Undocumented Z80 Documented" (Sean Young) and "Z80 MEMPTR"
// (boo_boo, Vladimir Kladov).
#include "Z80.h"

#include <string.h>


namespace {

uint8_t sSZP[256];	// S, Z, Y, X, P flags of a value
uint8_t sSZ[256];	// S, Z, Y, X
bool sTablesReady = false;


void
InitTables()
{
	for (int i = 0; i < 256; i++) {
		int bits = 0;
		for (int b = 0; b < 8; b++)
			bits += (i >> b) & 1;
		uint8_t f = static_cast<uint8_t>(i & (Z80_SF | Z80_YF | Z80_XF));
		if (i == 0)
			f |= Z80_ZF;
		sSZ[i] = f;
		sSZP[i] = f | ((bits & 1) == 0 ? Z80_PF : 0);
	}
	sTablesReady = true;
}

}	// namespace


Z80::Z80(Z80Bus* bus)
	:
	eiPending(false),
	m1Wait(0),
	fBus(bus)
{
	if (!sTablesReady)
		InitTables();
	Reset();
}


void
Z80::Reset()
{
	memset(&r, 0, sizeof(r));
	r.af = 0xffff;
	r.sp = 0xffff;
	r.af2 = r.bc2 = r.de2 = r.hl2 = 0xffff;
	eiPending = false;
}


uint8_t
Z80::_Fetch()
{
	return fBus->Read(r.pc++);
}


uint16_t
Z80::_Fetch16()
{
	uint8_t lo = _Fetch();
	return static_cast<uint16_t>(lo | (_Fetch() << 8));
}


uint16_t
Z80::_Read16(uint16_t a)
{
	uint8_t lo = _Read(a);
	return static_cast<uint16_t>(lo | (_Read(static_cast<uint16_t>(a + 1)) << 8));
}


void
Z80::_Write16(uint16_t a, uint16_t v)
{
	_Write(a, static_cast<uint8_t>(v));
	_Write(static_cast<uint16_t>(a + 1), static_cast<uint8_t>(v >> 8));
}


void
Z80::_Push(uint16_t v)
{
	r.sp -= 2;
	_Write16(r.sp, v);
}


uint16_t
Z80::_Pop()
{
	uint16_t v = _Read16(r.sp);
	r.sp += 2;
	return v;
}


static inline void
IncR(Z80Regs& r)
{
	r.r = static_cast<uint8_t>((r.r & 0x80) | ((r.r + 1) & 0x7f));
}


uint8_t&
Z80::_Reg8(int index, int prefix)
{
	switch (index) {
		case 0: return r.b;
		case 1: return r.c;
		case 2: return r.d;
		case 3: return r.e;
		case 4: return prefix == 0xdd ? r.ixh : (prefix == 0xfd ? r.iyh : r.h);
		case 5: return prefix == 0xdd ? r.ixl : (prefix == 0xfd ? r.iyl : r.l);
		default: return r.a;
	}
}


uint16_t&
Z80::_Reg16(int index, int prefix)
{
	switch (index) {
		case 0: return r.bc;
		case 1: return r.de;
		case 2: return prefix == 0xdd ? r.ix : (prefix == 0xfd ? r.iy : r.hl);
		default: return r.sp;
	}
}


uint16_t
Z80::_IndexAddress(int prefix)
{
	if (prefix == 0)
		return r.hl;
	int8_t d = static_cast<int8_t>(_Fetch());
	uint16_t a = static_cast<uint16_t>((prefix == 0xdd ? r.ix : r.iy) + d);
	r.wz = a;
	return a;
}


bool
Z80::_Cond(int c)
{
	switch (c) {
		case 0: return (r.f & Z80_ZF) == 0;
		case 1: return (r.f & Z80_ZF) != 0;
		case 2: return (r.f & Z80_CF) == 0;
		case 3: return (r.f & Z80_CF) != 0;
		case 4: return (r.f & Z80_PF) == 0;
		case 5: return (r.f & Z80_PF) != 0;
		case 6: return (r.f & Z80_SF) == 0;
		default: return (r.f & Z80_SF) != 0;
	}
}


uint8_t
Z80::_Inc(uint8_t v)
{
	uint8_t res = static_cast<uint8_t>(v + 1);
	r.f = static_cast<uint8_t>((r.f & Z80_CF) | sSZ[res] | ((v & 0x0f) == 0x0f ? Z80_HF : 0)
		| (v == 0x7f ? Z80_PF : 0));
	return res;
}


uint8_t
Z80::_Dec(uint8_t v)
{
	uint8_t res = static_cast<uint8_t>(v - 1);
	r.f = static_cast<uint8_t>((r.f & Z80_CF) | sSZ[res] | Z80_NF
		| ((v & 0x0f) == 0 ? Z80_HF : 0) | (v == 0x80 ? Z80_PF : 0));
	return res;
}


void
Z80::_Alu(int op, uint8_t v)
{
	uint8_t a = r.a;
	int res;
	switch (op) {
		case 0:	// ADD
		case 1:	// ADC
		{
			int c = op == 1 ? (r.f & Z80_CF) : 0;
			res = a + v + c;
			uint8_t r8 = static_cast<uint8_t>(res);
			r.f = static_cast<uint8_t>(sSZ[r8] | ((a ^ v ^ res) & Z80_HF)
				| ((((a ^ ~v) & (a ^ res) & 0x80) != 0) ? Z80_PF : 0)
				| (res > 0xff ? Z80_CF : 0));
			r.a = r8;
			break;
		}
		case 2:	// SUB
		case 3:	// SBC
		case 7:	// CP
		{
			int c = op == 3 ? (r.f & Z80_CF) : 0;
			res = a - v - c;
			uint8_t r8 = static_cast<uint8_t>(res);
			uint8_t f = static_cast<uint8_t>((sSZ[r8] & (Z80_SF | Z80_ZF)) | Z80_NF
				| ((a ^ v ^ res) & Z80_HF)
				| ((((a ^ v) & (a ^ res) & 0x80) != 0) ? Z80_PF : 0)
				| (((res & 0x100) != 0) ? Z80_CF : 0));
			if (op == 7)
				f |= v & (Z80_YF | Z80_XF);
			else {
				f |= r8 & (Z80_YF | Z80_XF);
				r.a = r8;
			}
			r.f = f;
			break;
		}
		case 4:	// AND
			r.a = a & v;
			r.f = static_cast<uint8_t>(sSZP[r.a] | Z80_HF);
			break;
		case 5:	// XOR
			r.a = a ^ v;
			r.f = sSZP[r.a];
			break;
		default:	// OR
			r.a = a | v;
			r.f = sSZP[r.a];
			break;
	}
}


uint8_t
Z80::_Rot(int op, uint8_t v)
{
	uint8_t res, c;
	switch (op) {
		case 0: c = v >> 7; res = static_cast<uint8_t>((v << 1) | c); break;	// RLC
		case 1: c = v & 1; res = static_cast<uint8_t>((v >> 1) | (c << 7)); break;	// RRC
		case 2: c = v >> 7; res = static_cast<uint8_t>((v << 1) | (r.f & Z80_CF)); break;	// RL
		case 3:	// RR
			c = v & 1;
			res = static_cast<uint8_t>((v >> 1) | ((r.f & Z80_CF) << 7));
			break;
		case 4: c = v >> 7; res = static_cast<uint8_t>(v << 1); break;	// SLA
		case 5: c = v & 1; res = static_cast<uint8_t>((v >> 1) | (v & 0x80)); break;	// SRA
		case 6: c = v >> 7; res = static_cast<uint8_t>((v << 1) | 1); break;	// SLL
		default: c = v & 1; res = static_cast<uint8_t>(v >> 1); break;	// SRL
	}
	r.f = static_cast<uint8_t>(sSZP[res] | c);
	return res;
}


void
Z80::_Add16(uint16_t& d, uint16_t v)
{
	uint32_t res = static_cast<uint32_t>(d) + v;
	r.wz = static_cast<uint16_t>(d + 1);
	r.f = static_cast<uint8_t>((r.f & (Z80_SF | Z80_ZF | Z80_PF))
		| (((d ^ v ^ res) >> 8) & Z80_HF)
		| ((res >> 8) & (Z80_YF | Z80_XF))
		| (res > 0xffff ? Z80_CF : 0));
	d = static_cast<uint16_t>(res);
}


void
Z80::_Adc16(uint16_t v)
{
	uint16_t hl = r.hl;
	uint32_t res = static_cast<uint32_t>(hl) + v + (r.f & Z80_CF);
	r.wz = static_cast<uint16_t>(hl + 1);
	uint16_t r16 = static_cast<uint16_t>(res);
	r.f = static_cast<uint8_t>(((r16 >> 8) & (Z80_SF | Z80_YF | Z80_XF))
		| (r16 == 0 ? Z80_ZF : 0)
		| (((hl ^ v ^ res) >> 8) & Z80_HF)
		| (((((hl ^ ~v) & (hl ^ res)) & 0x8000) != 0u) ? Z80_PF : 0)
		| (res > 0xffff ? Z80_CF : 0));
	r.hl = r16;
}


void
Z80::_Sbc16(uint16_t v)
{
	uint16_t hl = r.hl;
	uint32_t res = static_cast<uint32_t>(hl) - v - (r.f & Z80_CF);
	r.wz = static_cast<uint16_t>(hl + 1);
	uint16_t r16 = static_cast<uint16_t>(res);
	r.f = static_cast<uint8_t>(((r16 >> 8) & (Z80_SF | Z80_YF | Z80_XF)) | Z80_NF
		| (r16 == 0 ? Z80_ZF : 0)
		| (((hl ^ v ^ res) >> 8) & Z80_HF)
		| (((((hl ^ v) & (hl ^ res)) & 0x8000) != 0u) ? Z80_PF : 0)
		| (((res & 0x10000) != 0u) ? Z80_CF : 0));
	r.hl = r16;
}


int
Z80::Interrupt()
{
	if (!r.iff1 || eiPending)
		return 0;
	r.iff1 = r.iff2 = false;
	if (r.halted) {
		r.halted = false;
		r.pc++;
	}
	IncR(r);
	_Push(r.pc);
	if (r.im == 2) {
		r.pc = _Read16(static_cast<uint16_t>((r.i << 8) | 0xff));
		r.wz = r.pc;
		return 19 + 2 * m1Wait;
	}
	r.pc = 0x38;
	r.wz = r.pc;
	return 13 + 2 * m1Wait;
}


int
Z80::Step()
{
	eiPending = false;
	if (r.halted) {
		IncR(r);
		return 4 + m1Wait;
	}
	uint8_t r0 = r.r;
	int cycles = _StepInner();
	if (m1Wait != 0)
		cycles += m1Wait * ((r.r - r0) & 0x7f);
	return cycles;
}


int
Z80::_StepInner()
{

	int prefix = 0;
	int cycles = 0;
	uint8_t op = _Fetch();
	IncR(r);
	while (op == 0xdd || op == 0xfd) {
		prefix = op;
		cycles += 4;
		op = _Fetch();
		IncR(r);
	}
	if (op == 0xed)
		return cycles + _ExecED();
	if (op == 0xcb)
		return cycles + _ExecCB(prefix);
	return cycles + _ExecMain(op, prefix);
}


int
Z80::_ExecMain(uint8_t op, int prefix)
{
	int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;

	if (x == 1) {
		if (y == 6 && z == 6) {	// HALT
			r.halted = true;
			r.pc--;
			return 4;
		}
		if (y == 6) {	// LD (HL),r
			uint16_t a = _IndexAddress(prefix);
			_Write(a, _Reg8(z, 0));
			return (prefix != 0) ? 15 : 7;
		}
		if (z == 6) {	// LD r,(HL)
			uint16_t a = _IndexAddress(prefix);
			_Reg8(y, 0) = _Read(a);
			return (prefix != 0) ? 15 : 7;
		}
		_Reg8(y, prefix) = _Reg8(z, prefix);
		return 4;
	}

	if (x == 2) {
		if (z == 6) {
			uint16_t a = _IndexAddress(prefix);
			_Alu(y, _Read(a));
			return (prefix != 0) ? 15 : 7;
		}
		_Alu(y, _Reg8(z, prefix));
		return 4;
	}

	if (x == 0) {
		switch (z) {
			case 0:
				switch (y) {
					case 0: return 4;
					case 1: {
						uint16_t t = r.af; r.af = r.af2; r.af2 = t;
						return 4;
					}
					case 2: {
						int8_t d = static_cast<int8_t>(_Fetch());
						if (--r.b != 0) {
							r.pc = static_cast<uint16_t>(r.pc + d);
							r.wz = r.pc;
							return 13;
						}
						return 8;
					}
					case 3: {
						int8_t d = static_cast<int8_t>(_Fetch());
						r.pc = static_cast<uint16_t>(r.pc + d);
						r.wz = r.pc;
						return 12;
					}
					default: {
						int8_t d = static_cast<int8_t>(_Fetch());
						if (_Cond(y - 4)) {
							r.pc = static_cast<uint16_t>(r.pc + d);
							r.wz = r.pc;
							return 12;
						}
						return 7;
					}
				}
			case 1:
				if (q == 0) {
					_Reg16(p, prefix) = _Fetch16();
					return 10;
				}
				_Add16(_Reg16(2, prefix), _Reg16(p, prefix));
				return 11;
			case 2:
				switch (y) {
					case 0:
						_Write(r.bc, r.a);
						r.wz = static_cast<uint16_t>((r.a << 8) | ((r.bc + 1) & 0xff));
						return 7;
					case 1:
						r.a = _Read(r.bc);
						r.wz = static_cast<uint16_t>(r.bc + 1);
						return 7;
					case 2:
						_Write(r.de, r.a);
						r.wz = static_cast<uint16_t>((r.a << 8) | ((r.de + 1) & 0xff));
						return 7;
					case 3:
						r.a = _Read(r.de);
						r.wz = static_cast<uint16_t>(r.de + 1);
						return 7;
					case 4: {
						uint16_t a = _Fetch16();
						_Write16(a, _Reg16(2, prefix));
						r.wz = static_cast<uint16_t>(a + 1);
						return 16;
					}
					case 5: {
						uint16_t a = _Fetch16();
						_Reg16(2, prefix) = _Read16(a);
						r.wz = static_cast<uint16_t>(a + 1);
						return 16;
					}
					case 6: {
						uint16_t a = _Fetch16();
						_Write(a, r.a);
						r.wz = static_cast<uint16_t>((r.a << 8) | ((a + 1) & 0xff));
						return 13;
					}
					default: {
						uint16_t a = _Fetch16();
						r.a = _Read(a);
						r.wz = static_cast<uint16_t>(a + 1);
						return 13;
					}
				}
			case 3:
				if (q == 0)
					_Reg16(p, prefix)++;
				else
					_Reg16(p, prefix)--;
				return 6;
			case 4:
				if (y == 6) {
					uint16_t a = _IndexAddress(prefix);
					_Write(a, _Inc(_Read(a)));
					return (prefix != 0) ? 19 : 11;
				}
				_Reg8(y, prefix) = _Inc(_Reg8(y, prefix));
				return 4;
			case 5:
				if (y == 6) {
					uint16_t a = _IndexAddress(prefix);
					_Write(a, _Dec(_Read(a)));
					return (prefix != 0) ? 19 : 11;
				}
				_Reg8(y, prefix) = _Dec(_Reg8(y, prefix));
				return 4;
			case 6:
				if (y == 6) {
					uint16_t a = _IndexAddress(prefix);
					_Write(a, _Fetch());
					return (prefix != 0) ? 15 : 10;
				}
				_Reg8(y, prefix) = _Fetch();
				return 7;
			default: {
				uint8_t a = r.a, c;
				uint8_t keep = static_cast<uint8_t>(r.f & (Z80_SF | Z80_ZF | Z80_PF));
				switch (y) {
					case 0:	// RLCA
						c = a >> 7;
						r.a = static_cast<uint8_t>((a << 1) | c);
						r.f = static_cast<uint8_t>(keep | (r.a & (Z80_YF | Z80_XF)) | c);
						break;
					case 1:	// RRCA
						c = a & 1;
						r.a = static_cast<uint8_t>((a >> 1) | (c << 7));
						r.f = static_cast<uint8_t>(keep | (r.a & (Z80_YF | Z80_XF)) | c);
						break;
					case 2:	// RLA
						c = a >> 7;
						r.a = static_cast<uint8_t>((a << 1) | (r.f & Z80_CF));
						r.f = static_cast<uint8_t>(keep | (r.a & (Z80_YF | Z80_XF)) | c);
						break;
					case 3:	// RRA
						c = a & 1;
						r.a = static_cast<uint8_t>((a >> 1) | ((r.f & Z80_CF) << 7));
						r.f = static_cast<uint8_t>(keep | (r.a & (Z80_YF | Z80_XF)) | c);
						break;
					case 4: {	// DAA
						uint8_t corr = 0, carry = r.f & Z80_CF;
						if (((r.f & Z80_HF) != 0) || (a & 0x0f) > 9)
							corr |= 0x06;
						if ((carry != 0u) || a > 0x99) {
							corr |= 0x60;
							carry = Z80_CF;
						}
						uint8_t h;
						if ((r.f & Z80_NF) != 0) {
							h = (((r.f & Z80_HF) != 0) && (a & 0x0f) < 6) ? Z80_HF : 0;
							r.a = static_cast<uint8_t>(a - corr);
						} else {
							h = (a & 0x0f) > 9 ? Z80_HF : 0;
							r.a = static_cast<uint8_t>(a + corr);
						}
						r.f = static_cast<uint8_t>(sSZP[r.a] | h | (r.f & Z80_NF) | carry);
						break;
					}
					case 5:	// CPL
						r.a = static_cast<uint8_t>(~a);
						r.f = static_cast<uint8_t>((r.f & (Z80_SF | Z80_ZF | Z80_PF | Z80_CF))
							| Z80_HF | Z80_NF | (r.a & (Z80_YF | Z80_XF)));
						break;
					case 6:	// SCF
						r.f = static_cast<uint8_t>(keep | (a & (Z80_YF | Z80_XF)) | Z80_CF);
						break;
					default:	// CCF
						r.f = static_cast<uint8_t>(keep | (a & (Z80_YF | Z80_XF))
							| (((r.f & Z80_CF) != 0) ? Z80_HF : Z80_CF));
						break;
				}
				return 4;
			}
		}
	}

	// x == 3
	switch (z) {
		case 0:
			if (_Cond(y)) {
				r.pc = _Pop();
				r.wz = r.pc;
				return 11;
			}
			return 5;
		case 1:
			if (q == 0) {
				uint16_t v = _Pop();
				if (p == 3)
					r.af = v;
				else
					_Reg16(p, prefix) = v;
				return 10;
			}
			switch (p) {
				case 0:
					r.pc = _Pop();
					r.wz = r.pc;
					return 10;
				case 1: {
					uint16_t t;
					t = r.bc; r.bc = r.bc2; r.bc2 = t;
					t = r.de; r.de = r.de2; r.de2 = t;
					t = r.hl; r.hl = r.hl2; r.hl2 = t;
					return 4;
				}
				case 2:
					r.pc = _Reg16(2, prefix);
					return 4;
				default:
					r.sp = _Reg16(2, prefix);
					return 6;
			}
		case 2: {
			uint16_t a = _Fetch16();
			r.wz = a;
			if (_Cond(y))
				r.pc = a;
			return 10;
		}
		case 3:
			switch (y) {
				case 0:
					r.pc = _Fetch16();
					r.wz = r.pc;
					return 10;
				case 2: {
					uint8_t n = _Fetch();
					fBus->Out(static_cast<uint16_t>((r.a << 8) | n), r.a);
					r.wz = static_cast<uint16_t>((r.a << 8) | ((n + 1) & 0xff));
					return 11;
				}
				case 3: {
					uint8_t n = _Fetch();
					uint16_t port = static_cast<uint16_t>((r.a << 8) | n);
					r.a = fBus->In(port);
					r.wz = static_cast<uint16_t>(port + 1);
					return 11;
				}
				case 4: {
					uint16_t& rr = _Reg16(2, prefix);
					uint16_t v = _Read16(r.sp);
					_Write16(r.sp, rr);
					rr = v;
					r.wz = v;
					return 19;
				}
				case 5: {
					uint16_t t = r.de; r.de = r.hl; r.hl = t;
					return 4;
				}
				case 6:
					r.iff1 = r.iff2 = false;
					return 4;
				default:
					r.iff1 = r.iff2 = true;
					eiPending = true;
					return 4;
			}
		case 4: {
			uint16_t a = _Fetch16();
			r.wz = a;
			if (_Cond(y)) {
				_Push(r.pc);
				r.pc = a;
				return 17;
			}
			return 10;
		}
		case 5:
			if (q == 0) {
				_Push(p == 3 ? r.af : _Reg16(p, prefix));
				return 11;
			}
			{
				// p == 0: CALL nn (prefixes handled in Step)
				uint16_t a = _Fetch16();
				r.wz = a;
				_Push(r.pc);
				r.pc = a;
				return 17;
			}
		case 6:
			_Alu(y, _Fetch());
			return 7;
		default:
			_Push(r.pc);
			r.pc = static_cast<uint16_t>(y * 8);
			r.wz = r.pc;
			return 11;
	}
}


int
Z80::_ExecCB(int prefix)
{
	uint16_t address = 0;
	if (prefix != 0)
		address = _IndexAddress(prefix);
	uint8_t op = _Fetch();
	if (prefix == 0)
		IncR(r);
	int x = op >> 6, y = (op >> 3) & 7, z = op & 7;

	bool memory = (prefix != 0) || z == 6;
	if ((prefix == 0) && z == 6) {
		address = r.hl;
	}
	uint8_t v = memory ? _Read(address) : _Reg8(z, 0);

	if (x == 1) {	// BIT
		uint8_t bit = v & (1 << y);
		uint8_t f = static_cast<uint8_t>((r.f & Z80_CF) | Z80_HF
			| ((bit != 0u) ? 0 : (Z80_ZF | Z80_PF))
			| (y == 7 && (bit != 0u) ? Z80_SF : 0));
		if (memory)
			f |= (r.wz >> 8) & (Z80_YF | Z80_XF);
		else
			f |= v & (Z80_YF | Z80_XF);
		r.f = f;
		return (prefix != 0) ? 16 : (memory ? 12 : 8);
	}

	uint8_t res;
	if (x == 0)
		res = _Rot(y, v);
	else if (x == 2)
		res = static_cast<uint8_t>(v & ~(1 << y));
	else
		res = static_cast<uint8_t>(v | (1 << y));

	if (memory) {
		_Write(address, res);
		if ((prefix != 0) && z != 6)
			_Reg8(z, 0) = res;	// undocumented copy to a register
		return (prefix != 0) ? 19 : 15;
	}
	_Reg8(z, 0) = res;
	return 8;
}


int
Z80::_ExecED()
{
	uint8_t op = _Fetch();
	IncR(r);
	int x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;

	if (x == 1) {
		switch (z) {
			case 0: {
				uint8_t v = fBus->In(r.bc);
				r.wz = static_cast<uint16_t>(r.bc + 1);
				if (y != 6)
					_Reg8(y, 0) = v;
				r.f = static_cast<uint8_t>((r.f & Z80_CF) | sSZP[v]);
				return 12;
			}
			case 1:
				fBus->Out(r.bc, y == 6 ? 0 : _Reg8(y, 0));
				r.wz = static_cast<uint16_t>(r.bc + 1);
				return 12;
			case 2:
				if (q == 0)
					_Sbc16(_Reg16(p, 0));
				else
					_Adc16(_Reg16(p, 0));
				return 15;
			case 3: {
				uint16_t a = _Fetch16();
				if (q == 0)
					_Write16(a, _Reg16(p, 0));
				else
					_Reg16(p, 0) = _Read16(a);
				r.wz = static_cast<uint16_t>(a + 1);
				return 20;
			}
			case 4: {
				uint8_t a = r.a;
				r.a = 0;
				_Alu(2, a);
				return 8;
			}
			case 5:
				r.iff1 = r.iff2;
				r.pc = _Pop();
				r.wz = r.pc;
				return 14;
			case 6: {
				static const uint8_t kModes[8] = { 0, 0, 1, 2, 0, 0, 1, 2 };
				r.im = kModes[y];
				return 8;
			}
			default:
				switch (y) {
					case 0: r.i = r.a; return 9;
					case 1: r.r = r.a; return 9;
					case 2:
					case 3:
						r.a = y == 2 ? r.i : r.r;
						r.f = static_cast<uint8_t>((r.f & Z80_CF) | sSZ[r.a]
							| (r.iff2 ? Z80_PF : 0));
						return 9;
					case 4: {	// RRD
						uint8_t m = _Read(r.hl);
						_Write(r.hl, static_cast<uint8_t>((r.a << 4) | (m >> 4)));
						r.a = static_cast<uint8_t>((r.a & 0xf0) | (m & 0x0f));
						r.f = static_cast<uint8_t>((r.f & Z80_CF) | sSZP[r.a]);
						r.wz = static_cast<uint16_t>(r.hl + 1);
						return 18;
					}
					case 5: {	// RLD
						uint8_t m = _Read(r.hl);
						_Write(r.hl, static_cast<uint8_t>((m << 4) | (r.a & 0x0f)));
						r.a = static_cast<uint8_t>((r.a & 0xf0) | (m >> 4));
						r.f = static_cast<uint8_t>((r.f & Z80_CF) | sSZP[r.a]);
						r.wz = static_cast<uint16_t>(r.hl + 1);
						return 18;
					}
					default:
						return 8;
				}
		}
	}

	if (x == 2 && z <= 3 && y >= 4) {
		bool dec = (y & 1) != 0;
		bool repeat = y >= 6;
		int step = dec ? -1 : 1;
		switch (z) {
			case 0: {	// LDI/LDD/LDIR/LDDR
				uint8_t v = _Read(r.hl);
				_Write(r.de, v);
				r.hl = static_cast<uint16_t>(r.hl + step);
				r.de = static_cast<uint16_t>(r.de + step);
				r.bc--;
				uint8_t n = static_cast<uint8_t>(v + r.a);
				r.f = static_cast<uint8_t>((r.f & (Z80_SF | Z80_ZF | Z80_CF))
					| (n & Z80_XF) | ((n << 4) & Z80_YF)
					| ((r.bc != 0u) ? Z80_PF : 0));
				if (repeat && (r.bc != 0u)) {
					r.pc -= 2;
					r.wz = static_cast<uint16_t>(r.pc + 1);
					return 21;
				}
				return 16;
			}
			case 1: {	// CPI/CPD/CPIR/CPDR
				uint8_t v = _Read(r.hl);
				uint8_t res = static_cast<uint8_t>(r.a - v);
				r.hl = static_cast<uint16_t>(r.hl + step);
				r.bc--;
				r.wz = static_cast<uint16_t>(r.wz + step);
				uint8_t h = static_cast<uint8_t>((r.a ^ v ^ res) & Z80_HF);
				uint8_t n = static_cast<uint8_t>(res - ((h != 0u) ? 1 : 0));
				r.f = static_cast<uint8_t>((r.f & Z80_CF) | (sSZ[res] & (Z80_SF | Z80_ZF))
					| h | Z80_NF | (n & Z80_XF) | ((n << 4) & Z80_YF)
					| ((r.bc != 0u) ? Z80_PF : 0));
				if (repeat && (r.bc != 0u) && res != 0) {
					r.pc -= 2;
					r.wz = static_cast<uint16_t>(r.pc + 1);
					return 21;
				}
				return 16;
			}
			case 2: {	// INI/IND/INIR/INDR
				uint8_t v = fBus->In(r.bc);
				r.wz = static_cast<uint16_t>(r.bc + step);
				_Write(r.hl, v);
				r.hl = static_cast<uint16_t>(r.hl + step);
				r.b--;
				int t = v + static_cast<uint8_t>(r.c + step);
				uint8_t f = sSZ[r.b];
				if ((v & 0x80) != 0)
					f |= Z80_NF;
				if (t > 0xff)
					f |= Z80_HF | Z80_CF;
				f |= sSZP[static_cast<uint8_t>((t & 7) ^ r.b)] & Z80_PF;
				r.f = f;
				if (repeat && (r.b != 0u)) {
					r.pc -= 2;
					return 21;
				}
				return 16;
			}
			default: {	// OUTI/OUTD/OTIR/OTDR
				uint8_t v = _Read(r.hl);
				r.b--;
				r.wz = static_cast<uint16_t>(r.bc + step);
				fBus->Out(r.bc, v);
				r.hl = static_cast<uint16_t>(r.hl + step);
				int t = v + r.l;
				uint8_t f = sSZ[r.b];
				if ((v & 0x80) != 0)
					f |= Z80_NF;
				if (t > 0xff)
					f |= Z80_HF | Z80_CF;
				f |= sSZP[static_cast<uint8_t>((t & 7) ^ r.b)] & Z80_PF;
				r.f = f;
				if (repeat && (r.b != 0u)) {
					r.pc -= 2;
					return 21;
				}
				return 16;
			}
		}
	}
	return 8;	// undefined ED opcodes act as NOP
}
