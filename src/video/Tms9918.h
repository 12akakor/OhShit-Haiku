/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_TMS9918_H
#define OS_TMS9918_H


// Texas Instruments TMS9918A (MSX1 VDP), for the reference machine:
// - ports #98 (VRAM data) and #99 (address, registers, status)
// - 16 KB VRAM, the fixed 16 colour palette
// - screen modes Graphic 1, Graphic 2, Text 1 and Multicolour
// - sprites 8x8 or 16x16, magnified or not, 4 per line, fifth sprite
//   flag and number, collision flag
// - vertical blank interrupt (status bit 7, register 1 bit 5)

#include <stdint.h>


class Tms9918 {
public:
	enum {
		kWidth = 256,
		kLines = 192,
		kLinesPerFrame = 262
	};

							Tms9918();
			void			Reset();

			uint8_t			ReadPort(int port);
			void			WritePort(int port, uint8_t value);

			void			WriteRegister(int reg, uint8_t value);
			uint8_t			Register(int reg) const { return fReg[reg & 7]; }
			uint8_t			Status() const { return fStatus; }

	// Renders one display line (0..191) into the frame.
			void			RenderLine(int line);
	// Start of the vertical blank: sets the interrupt flag.
			void			StartVBlank();
			bool			InterruptPending() const
								{ return (fStatus & 0x80) && (fReg[1] & 0x20); }

	// Colour index (0..15) per pixel, kWidth x kLines.
			const uint8_t*	Frame() const { return fFrame; }
	static	uint32_t		Rgb(int index);

			const uint8_t*	Vram() const { return fVram; }
			uint8_t*		VramWritable() { return fVram; }

	// Shows all sprites of a line, not only the first 4 (no flicker; the
	// status register still reports the fifth sprite as the chip does).
			void			SetSpriteLimit(bool limit) { fSpriteLimit = limit; }
			uint16_t		Address() const { return fAddress; }

private:
			void			_RenderSprites(int line, uint8_t* out);

			uint8_t			fVram[0x4000];
			uint8_t			fReg[8];
			uint8_t			fStatus;
			uint16_t		fAddress;
			uint8_t			fReadAhead;
			uint8_t			fLatch;
			bool			fLatchFull;
			bool			fSpriteLimit = true;
			uint8_t			fFrame[kWidth * kLines];
};


#endif	// OS_TMS9918_H
