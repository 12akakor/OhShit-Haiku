/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// AY-3-8910, see psg.h.
//
// Tone: the output toggles every 'period' steps of clock/8, so the
// frequency is clock / (16 * period). Noise: 17 bit LFSR clocked at
// clock / (16 * period). Envelope: one of 16 levels per 'period' steps of
// clock / 16, shapes from register 13 (CONT, ATT, ALT, HOLD).
#include "Psg.h"

#include <string.h>


namespace {

const double kClock = 3579545.0 / 2.0;

// AY amplitude table (relative), as measured on real chips.
const double kVolume[16] = {
	0.0, 0.0137, 0.0205, 0.0291, 0.0423, 0.0618, 0.0847, 0.1369,
	0.1691, 0.2647, 0.3527, 0.4499, 0.5704, 0.6873, 0.8482, 1.0
};

}	// namespace


Psg::Psg()
{
	Reset();
}


void
Psg::Reset()
{
	memset(fReg, 0, sizeof(fReg));
	fReg[7] = 0xff;
	for (int i = 0; i < 3; i++) {
		fToneCount[i] = 0;
		fToneOut[i] = 0;
	}
	fNoiseCount = 0;
	fLfsr = 1;
	fNoiseOut = 0;
	fNoiseHalf = 0;
	fEnvCount = 0;
	fEnvStep = 0;
	fEnvHolding = false;
	fEnvUp = false;
	fEnvVolume = 0;
	fDivider = 0;
	fPhase = 0.0;
}


void
Psg::Write(int reg, uint8_t value)
{
	reg &= 15;
	static const uint8_t kMask[16] = {
		0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0xff,
		0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff
	};
	fReg[reg] = value & kMask[reg];
	if (reg == 13) {
		fEnvStep = 0;
		fEnvCount = 0;
		fEnvHolding = false;
		fEnvUp = (value & 4) != 0;
		fEnvVolume = fEnvUp ? 0 : 15;
	}
}


void
Psg::_Tick()
{
	for (int c = 0; c < 3; c++) {
		int period = fReg[c * 2] | (fReg[c * 2 + 1] << 8);
		if (period == 0)
			period = 1;
		if (++fToneCount[c] >= period) {
			fToneCount[c] = 0;
			fToneOut[c] ^= 1;
		}
	}

	// Noise and envelope run at clock / 16.
	fDivider ^= 1;
	if (fDivider != 0)
		return;

	int np = fReg[6] & 0x1f;
	if (np == 0)
		np = 1;
	if (++fNoiseCount >= np) {
		fNoiseCount = 0;
		fNoiseHalf ^= 1;
		if (fNoiseHalf != 0) {
			// 17 bit LFSR, taps 0 and 3
			uint32_t bit = (fLfsr ^ (fLfsr >> 3)) & 1;
			fLfsr = (fLfsr >> 1) | (bit << 16);
			fNoiseOut = fLfsr & 1;
		}
	}

	int ep = fReg[11] | (fReg[12] << 8);
	if (ep == 0)
		ep = 1;
	if (!fEnvHolding && ++fEnvCount >= ep) {
		fEnvCount = 0;
		if (++fEnvStep >= 16) {
			int shape = fReg[13];
			bool cont = (shape & 8) != 0;
			bool alt = (shape & 2) != 0;
			bool hold = (shape & 1) != 0;
			if (!cont) {
				fEnvHolding = true;
				fEnvVolume = 0;
				return;
			}
			if (hold) {
				fEnvHolding = true;
				fEnvVolume = (alt ? !fEnvUp : fEnvUp) ? 15 : 0;
				return;
			}
			if (alt)
				fEnvUp = !fEnvUp;
			fEnvStep = 0;
		}
	}
	if (!fEnvHolding)
		fEnvVolume = fEnvUp ? fEnvStep : 15 - fEnvStep;
}


void
Psg::Render(int16_t* out, int count, int rate)
{
	const double stepsPerSample = (kClock / 8.0) / rate;
	for (int i = 0; i < count; i++) {
		fPhase += stepsPerSample;
		double sum = 0.0;
		int n = 0;
		while (fPhase >= 1.0) {
			_Tick();
			fPhase -= 1.0;
			uint8_t mixer = fReg[7];
			double s = 0.0;
			for (int c = 0; c < 3; c++) {
				int tone = fToneOut[c] | ((mixer >> c) & 1);
				int noise = fNoiseOut | ((mixer >> (c + 3)) & 1);
				if ((tone & noise) != 0) {
					uint8_t amp = fReg[8 + c];
					int vol = ((amp & 0x10) != 0) ? fEnvVolume : (amp & 0x0f);
					s += kVolume[vol];
				}
			}
			sum += s;
			n++;
		}
		double v = n > 0 ? sum / n : 0.0;
		// Three full channels give about 3.0; leave headroom.
		int sample = static_cast<int>(v * 9000.0);
		if (sample > 32767)
			sample = 32767;
		out[i] = static_cast<int16_t>(sample);
	}
}
