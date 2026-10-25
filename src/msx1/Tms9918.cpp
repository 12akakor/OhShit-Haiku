/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// TMS9918A, see Tms9918.h.
#include "Tms9918.h"

#include <string.h>


namespace {

// The usual MSX1 colours (as in the TMS9918A data sheet, converted to RGB).
const uint32_t kPalette[16] = {
	0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d,
	0x42ebf5, 0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba,
	0xcccccc, 0xffffff
};

}	// namespace


Tms9918::Tms9918()
{
	Reset();
}


void
Tms9918::Reset()
{
	memset(fVram, 0, sizeof(fVram));
	memset(fReg, 0, sizeof(fReg));
	memset(fFrame, 0, sizeof(fFrame));
	fStatus = 0;
	fAddress = 0;
	fReadAhead = 0;
	fLatch = 0;
	fLatchFull = false;
}


uint32_t
Tms9918::Rgb(int index)
{
	return kPalette[index & 15];
}


uint8_t
Tms9918::ReadPort(int port)
{
	fLatchFull = false;
	if ((port & 1) == 0) {
		uint8_t value = fReadAhead;
		fReadAhead = fVram[fAddress];
		fAddress = (fAddress + 1) & 0x3fff;
		return value;
	}
	uint8_t value = fStatus;
	fStatus &= 0x1f;
	return value;
}


void
Tms9918::WritePort(int port, uint8_t value)
{
	if ((port & 1) == 0) {
		fLatchFull = false;
		fVram[fAddress] = value;
		fReadAhead = value;
		fAddress = (fAddress + 1) & 0x3fff;
		return;
	}
	if (!fLatchFull) {
		fLatch = value;
		fLatchFull = true;
		// The TMS99xx takes the low address byte at once.
		fAddress = (fAddress & 0x3f00) | value;
		return;
	}
	fLatchFull = false;
	if (value & 0x80) {
		WriteRegister(value & 7, fLatch);
		return;
	}
	fAddress = static_cast<uint16_t>(((value & 0x3f) << 8) | fLatch);
	if ((value & 0x40) == 0) {
		fReadAhead = fVram[fAddress];
		fAddress = (fAddress + 1) & 0x3fff;
	}
}


void
Tms9918::WriteRegister(int reg, uint8_t value)
{
	fReg[reg & 7] = value;
}


void
Tms9918::StartVBlank()
{
	fStatus |= 0x80;
}


void
Tms9918::RenderLine(int line)
{
	uint8_t* out = fFrame + line * kWidth;
	uint8_t backdrop = fReg[7] & 0x0f;
	if ((fReg[1] & 0x40) == 0) {
		memset(out, backdrop, kWidth);
		return;
	}
	bool m1 = fReg[1] & 0x10;	// text
	bool m2 = fReg[1] & 0x08;	// multicolour
	bool m3 = fReg[0] & 0x02;	// graphic 2
	uint16_t nameBase = (fReg[2] & 0x0f) << 10;
	int row = line >> 3;
	int y = line & 7;

	if (m1) {
		uint8_t fg = fReg[7] >> 4;
		uint8_t bg = backdrop;
		memset(out, bg, kWidth);
		uint16_t patBase = (fReg[4] & 0x07) << 11;
		for (int col = 0; col < 40; col++) {
			uint8_t ch = fVram[(nameBase + row * 40 + col) & 0x3fff];
			uint8_t bits;
			if (m3) {
				// Text 2-like mixed mode: not used by MSX1 software.
				bits = fVram[(patBase + ch * 8 + y) & 0x3fff];
			} else
				bits = fVram[(patBase + ch * 8 + y) & 0x3fff];
			for (int i = 0; i < 6; i++)
				out[8 + col * 6 + i] = (bits & (0x80 >> i)) ? fg : bg;
		}
		return;	// no sprites in text mode
	}

	for (int col = 0; col < 32; col++) {
		uint8_t ch = fVram[(nameBase + row * 32 + col) & 0x3fff];
		uint8_t bits, color;
		if (m2) {
			uint16_t patBase = (fReg[4] & 0x07) << 11;
			uint8_t c = fVram[(patBase + ch * 8 + ((row & 3) << 1)
				+ (y >> 2)) & 0x3fff];
			bits = 0xf0;
			color = c;
		} else if (m3) {
			int code = ch | ((row & 0x18) << 5);
			int offset = code * 8 + y;
			uint16_t pa = static_cast<uint16_t>((offset
				& (((fReg[4] & 0x03) << 11) | 0x7ff)) | ((fReg[4] & 0x04) << 11));
			uint16_t ca = static_cast<uint16_t>((offset
				& (((fReg[3] & 0x7f) << 6) | 0x3f)) | ((fReg[3] & 0x80) << 6));
			bits = fVram[pa & 0x3fff];
			color = fVram[ca & 0x3fff];
		} else {
			uint16_t patBase = (fReg[4] & 0x07) << 11;
			bits = fVram[(patBase + ch * 8 + y) & 0x3fff];
			color = fVram[((fReg[3] << 6) + (ch >> 3)) & 0x3fff];
		}
		uint8_t fg = color >> 4;
		uint8_t bg = color & 0x0f;
		if (fg == 0)
			fg = backdrop;
		if (bg == 0)
			bg = backdrop;
		for (int i = 0; i < 8; i++)
			out[col * 8 + i] = (bits & (0x80 >> i)) ? fg : bg;
	}
	_RenderSprites(line, out);
}


void
Tms9918::_RenderSprites(int line, uint8_t* out)
{
	uint16_t attrBase = (fReg[5] & 0x7f) << 7;
	uint16_t patBase = (fReg[6] & 0x07) << 11;
	bool size16 = fReg[1] & 0x02;
	bool mag = fReg[1] & 0x01;
	int size = (size16 ? 16 : 8) * (mag ? 2 : 1);

	int visible[4];
	int count = 0;
	int last = 0;
	for (int i = 0; i < 32; i++) {
		const uint8_t* a = fVram + ((attrBase + i * 4) & 0x3fff);
		last = i;
		if (a[0] == 208)
			break;
		int dy = (line - a[0] - 1) & 0xff;
		if (dy >= size)
			continue;
		if (count == 4) {
			if ((fStatus & 0x40) == 0)
				fStatus = static_cast<uint8_t>((fStatus & 0xe0) | 0x40 | i);
			break;
		}
		visible[count++] = i;
	}
	if ((fStatus & 0x40) == 0)
		fStatus = static_cast<uint8_t>((fStatus & 0xe0) | last);

	uint8_t owner[kWidth + 64];
	memset(owner, 0, sizeof(owner));
	for (int k = 0; k < count; k++) {
		const uint8_t* a = fVram + ((attrBase + visible[k] * 4) & 0x3fff);
		int dy = (line - a[0] - 1) & 0xff;
		if (mag)
			dy >>= 1;
		int x = a[1];
		if (a[3] & 0x80)
			x -= 32;
		uint8_t color = a[3] & 0x0f;
		uint8_t pat = size16 ? (a[2] & 0xfc) : a[2];
		uint16_t pa = static_cast<uint16_t>(patBase + pat * 8 + dy);
		uint16_t bits = static_cast<uint16_t>(fVram[pa & 0x3fff] << 8);
		if (size16)
			bits |= fVram[(pa + 16) & 0x3fff];
		int width = size16 ? 16 : 8;
		for (int i = 0; i < width * (mag ? 2 : 1); i++) {
			int bit = mag ? i >> 1 : i;
			if ((bits & (0x8000 >> bit)) == 0)
				continue;
			int px = x + i;
			if (px < 0 || px >= kWidth)
				continue;
			if (owner[px]) {
				fStatus |= 0x20;	// collision
				continue;
			}
			owner[px] = 1;
			if (color != 0)
				out[px] = color;
		}
	}
}
