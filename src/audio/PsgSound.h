/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_PSG_SOUND_H
#define OS_PSG_SOUND_H


// The sound of the game: an AY-3-8910 driven by the register writes of the
// program at their exact CPU cycle, so the 1 bit speech (the volume of
// channel C switched on and off thousands of times a second) comes out
// right. Output: stereo 16 bit at 44100 Hz.
//
// The chip is run at its own clock (CPU / 16 per step, 223.7 kHz); each
// output sample is the average of the steps it covers. A DC filter takes
// away the offset of the chip's positive-only output.

#include <stdint.h>

#include <vector>


class PsgSound {
public:
	enum { kRate = 44100 };

							PsgSound();
			void			Reset();

	// 0 mono, up to 1: channel A to the left, B to the right, C in the
	// middle.
			void			SetStereoWidth(float width);
	// Smooths the edges of the sound (a gentle low pass); off: as the
	// chip sounds.
			void			SetSmooth(bool smooth);

	// Writes a register at CPU cycle 'time' (times only go up).
			void			Write(uint64_t time, int reg, uint8_t value);
	// Renders up to CPU cycle 'time' and appends the samples (interleaved
	// left, right) to 'out'.
			void			RenderTo(uint64_t time, std::vector<int16_t>& out);

private:
			void			_Advance(uint64_t time, std::vector<int16_t>* out);
			void			_Step();
			void			_Emit(std::vector<int16_t>* out);

			uint8_t			fReg[16];
			int				fToneCount[3];
			int				fToneOut[3];
			int				fNoiseCount;
			int				fNoiseHalf;
			uint32_t		fLfsr;
			int				fNoiseOut;
			int				fEnvCount;
			int				fEnvStep;
			bool			fEnvHolding;
			bool			fEnvUp;
			int				fEnvVolume;
			int				fDivider;

			uint64_t		fTime;		// CPU cycle of the next chip step
			double			fSampleTime;	// CPU cycle where the next sample ends
			double			fSum[3];	// level sums of the steps in this sample
			int				fSteps;

			float			fWidth;
			bool			fSmooth;
			double			fDcLeft, fDcRight;	// DC filter state
			double			fLowLeft, fLowRight;	// low pass state
			std::vector<int16_t> fPending;	// samples done before a write
};


#endif	// OS_PSG_SOUND_H
