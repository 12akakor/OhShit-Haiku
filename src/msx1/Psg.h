/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_PSG_H
#define OS_PSG_H


// AY-3-8910 (MSX PSG): three tone channels, noise, envelope.
// Clock 1.7897725 MHz (MSX CPU clock / 2). Output: mono, 16 bit.

#include <stdint.h>


class Psg {
public:
							Psg();
			void			Reset();

			void			Write(int reg, uint8_t value);
			uint8_t			Register(int reg) const { return fReg[reg & 15]; }

	// Produces 'count' mono samples at 'rate' Hz.
			void			Render(int16_t* out, int count, int rate);

private:
			void			_Tick();	// one step of clock / 8

			uint8_t			fReg[16];
			int				fToneCount[3];
			int				fToneOut[3];
			int				fNoiseCount;
			uint32_t		fLfsr;
			int				fNoiseOut;
			int				fNoiseHalf;
			int				fEnvCount;
			int				fEnvStep;	// 0..31 within the current period
			bool			fEnvHolding;
			bool			fEnvUp;
			int				fEnvVolume;
			int				fDivider;
			double			fPhase;	// clock/8 steps owed
};


#endif	// OS_PSG_H
