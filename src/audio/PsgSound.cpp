/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// AY-3-8910 at cycle precision, see PsgSound.h.
#include "PsgSound.h"

#include <math.h>
#include <string.h>


namespace {

const double kCpuClock = 3579545.0;
const int kCyclesPerStep = 16;	// the chip clock is CPU / 2, steps are clock / 8
const double kCyclesPerSample = kCpuClock / PsgSound::kRate;

// AY amplitude per volume level (relative), as measured on real chips.
const double kVolume[16] = {
	0.0, 0.0137, 0.0205, 0.0291, 0.0423, 0.0618, 0.0847, 0.1369,
	0.1691, 0.2647, 0.3527, 0.4499, 0.5704, 0.6873, 0.8482, 1.0
};

}	// namespace


PsgSound::PsgSound()
	:
	fWidth(0.3f),
	fSmooth(false)
{
	Reset();
}


void
PsgSound::Reset()
{
	memset(fReg, 0, sizeof(fReg));
	fReg[7] = 0xbf;
	for (int c = 0; c < 3; c++) {
		fToneCount[c] = 0;
		fToneOut[c] = 0;
		fSum[c] = 0.0;
	}
	fNoiseCount = 0;
	fNoiseHalf = 0;
	fLfsr = 1;
	fNoiseOut = 0;
	fEnvCount = 0;
	fEnvStep = 0;
	fEnvHolding = true;
	fEnvUp = false;
	fEnvVolume = 0;
	fDivider = 0;
	fTime = 0;
	fSampleTime = kCyclesPerSample;
	fSteps = 0;
	fDcLeft = fDcRight = 0.0;
	fLowLeft = fLowRight = 0.0;
	fPending.clear();
}


void
PsgSound::SetStereoWidth(float width)
{
	fWidth = width < 0.0f ? 0.0f : (width > 1.0f ? 1.0f : width);
}


void
PsgSound::SetSmooth(bool smooth)
{
	fSmooth = smooth;
}


void
PsgSound::Write(uint64_t time, int reg, uint8_t value)
{
	_Advance(time, NULL);
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
PsgSound::RenderTo(uint64_t time, std::vector<int16_t>& out)
{
	_Advance(time, &out);
}


void
PsgSound::_Advance(uint64_t time, std::vector<int16_t>* out)
{
	// Samples finished before a register write wait in fPending for the
	// next RenderTo.
	if (out != NULL && !fPending.empty()) {
		out->insert(out->end(), fPending.begin(), fPending.end());
		fPending.clear();
	}
	std::vector<int16_t>* target = out != NULL ? out : &fPending;
	while (fTime + kCyclesPerStep <= time) {
		_Step();
		fTime += kCyclesPerStep;
		uint8_t mixer = fReg[7];
		for (int c = 0; c < 3; c++) {
			int tone = fToneOut[c] | ((mixer >> c) & 1);
			int noise = fNoiseOut | ((mixer >> (c + 3)) & 1);
			if ((tone & noise) != 0) {
				uint8_t amp = fReg[8 + c];
				int vol = (amp & 0x10) != 0 ? fEnvVolume : (amp & 0x0f);
				fSum[c] += kVolume[vol];
			}
		}
		fSteps++;
		if (static_cast<double>(fTime) >= fSampleTime) {
			_Emit(target);
			fSampleTime += kCyclesPerSample;
		}
	}
}


void
PsgSound::_Emit(std::vector<int16_t>* out)
{
	double a = fSteps > 0 ? fSum[0] / fSteps : 0.0;
	double b = fSteps > 0 ? fSum[1] / fSteps : 0.0;
	double c = fSteps > 0 ? fSum[2] / fSteps : 0.0;
	fSum[0] = fSum[1] = fSum[2] = 0.0;
	fSteps = 0;
	double w = fWidth;
	double left = a * (1.0 + w) + b * (1.0 - w) + c;
	double right = a * (1.0 - w) + b * (1.0 + w) + c;
	// DC filter (about 20 Hz)
	const double kDc = 0.9972;
	double dl = left - fDcLeft;
	double dr = right - fDcRight;
	fDcLeft += (1.0 - kDc) * dl;
	fDcRight += (1.0 - kDc) * dr;
	left = dl;
	right = dr;
	if (fSmooth) {
		const double kLow = 0.45;	// about 6 kHz
		fLowLeft += kLow * (left - fLowLeft);
		fLowRight += kLow * (right - fLowRight);
		left = fLowLeft;
		right = fLowRight;
	}
	int l = static_cast<int>(left * 7000.0);
	int r = static_cast<int>(right * 7000.0);
	l = l > 32767 ? 32767 : (l < -32768 ? -32768 : l);
	r = r > 32767 ? 32767 : (r < -32768 ? -32768 : r);
	out->push_back(static_cast<int16_t>(l));
	out->push_back(static_cast<int16_t>(r));
}


void
PsgSound::_Step()
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
	// Noise and envelope run at half that rate.
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
