/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Sound effects and speech (#85D8-#88A6).
//
// Channel A plays the chomp (with the envelope), channel B the siren (or
// the frightened and eyes sounds), channel C the eaten ghost sound and the
// speech. The speech is 1 bit: every byte of a sample is a duration, after
// which the volume of channel C flips between 15 and 0; a zero byte keeps
// the volume for one more duration of 256.
#include "Game.h"
#include "Timing.h"

#include "Bios.h"


using namespace sym;
using bios::RDPSG;
using bios::WRTPSG;


void
SilencePsg()
{
	Cost(timing::SILENCE_PSG, 0x85d8);
	uint8_t mixer = RDPSG(7) | 0x3f;
	for (uint8_t reg = 0; reg < 13; reg++)
		WRTPSG(reg, 0);
	WRTPSG(7, mixer);
}


void
StartSiren()
{
	Cost(timing::START_SIREN, 0x85f1);
	WRTPSG(10, 0x0a);
	WRTPSG(5, 0x00);
	WRTPSG(7, RDPSG(7) & 0xf9);
	M(SirenPitch) = 0x52;
	WRTPSG(9, 0x0c);
	WRTPSG(3, 0x00);
	M(SirenRising) = 0;
	M(SirenTone) = 0x80;
	M(SirenTimer) = 0xff;
	M(ToneTimer) = 0xff;
	M(FrightSound) = 0;
	M(ChompHigh) = 0;
	M(EatGhostSound) = 0;
	M(ChompOn) = 0;
}


namespace {

void
UpdateChomp()
{
	if (M(ChompOn) == 0) {
		Cost(31, 0x86f1);
		return;
	}
	Cost(200, 0x86f1);
	uint8_t a;
	if (M(ChompHigh) == 0) {
		a = static_cast<uint8_t>(M(ChompPitch) - 9);
		M(ChompPitch) = a;
		if (a < 0x70) {
			WRTPSG(8, 0);
			M(ChompOn) = 0;
			return;
		}
	} else {
		a = static_cast<uint8_t>(M(ChompPitch) + 0x0b);
		M(ChompPitch) = a;
		if (a >= 0xf0) {
			WRTPSG(8, 0);
			M(ChompOn) = 0;
			return;
		}
	}
	WRTPSG(0, a);
}


// From #8665: the tone of channel B (siren, frightened, eyes).
void
UpdateTone()
{
	if (++M(ToneTimer) != 0) {
		Cost(167, 0x863e);
		return;
	}
	Cost(384, 0x863e);
	M(ToneTimer) = 0xfd;
	uint8_t e;
	if (M(EyesSound) != 0) {
		uint8_t a = static_cast<uint8_t>(M(SirenTone) + 1);
		if (a >= 0x90)
			a = 0x80;
		M(SirenTone) = a;
		e = a;
	} else {
		if (M(SirenRising) == 0) {
			uint8_t a = static_cast<uint8_t>(M(SirenTone) - 1);
			M(SirenTone) = a;
			if (a < 0x60)
				M(SirenRising) = 1;
		} else {
			M(ToneTimer) = 0xfe;
			uint8_t a = static_cast<uint8_t>(M(SirenTone) + 3);
			M(SirenTone) = a;
			if (a >= 0x90) {
				M(SirenRising) = 0;
				M(SirenTone) = 0x80;
			}
		}
		e = static_cast<uint8_t>(M(SirenTone) + 0x10);
		if (M(FrightSound) != 0) {
			M(ToneTimer) = 0xff;
			e = static_cast<uint8_t>(e + 0x50);
		}
	}
	WRTPSG(2, e);
}

}	// namespace


void
UpdateSound()
{
	UpdateChomp();
	if (M(EatGhostSound) != 0) {
		if (++M(EatGhostTimer) == 0) {
			M(EatGhostTimer) = 0xfc;
			uint8_t pitch = M(EatGhostPitch);
			uint8_t a = static_cast<uint8_t>(pitch - 0x1e);
			M(EatGhostPitch) = a;
			if (pitch >= 0x1e) {
				Cost(245, 0x863e);
				WRTPSG(4, a);
				return;
			}
			Cost(300, 0x863e);
			M(EatGhostSound) = 0;
			WRTPSG(5, 0);
			WRTPSG(10, 0x0a);
			return;
		}
		UpdateTone();
		return;
	}
	if (++M(SirenTimer) == 0) {
		M(SirenTimer) = 0xfe;
		uint8_t a = static_cast<uint8_t>(M(SirenPitch) - 1);
		if (a == 0x32)
			a = 0x52;
		M(SirenPitch) = a;
		WRTPSG(4, static_cast<uint8_t>(a - 0x10));
	}
	UpdateTone();
}


void
FrightSoundOn()
{
	M(FrightSound) = 1;
}


void
FrightSoundOff()
{
	M(FrightSound) = 0;
}


void
EyesSoundOn()
{
	M(EyesSound) = 1;
	M(SirenTone) = 0x80;
}


void
EyesSoundOff()
{
	M(EyesSound) = 0;
	M(SirenRising) = 0;
	M(SirenTone) = 0x80;
}


void
PlayChomp()
{
	Cost(timing::PLAY_CHOMP, 0x8729);
	uint8_t high = M(ChompHigh) ^ 1;
	M(ChompHigh) = high;
	M(ChompPitch) = high != 0 ? 0x90 : 0xf0;
	M(ChompOn) = 1;
	WRTPSG(8, 0x10);
	WRTPSG(13, 13);
	WRTPSG(11, 0);
	WRTPSG(12, 0);
	WRTPSG(1, 1);
	WRTPSG(7, RDPSG(7) & 0xfe);
}


void
PlayEatGhostSound()
{
	Cost(100, 0x879a);
	M(EatGhostSound) = 1;
	M(EatGhostPitch) = 0xff;
	WRTPSG(5, 1);
	WRTPSG(10, 0x0f);
	M(EatGhostTimer) = 0xff;
}


namespace {

void
PlaySample(uint8_t sample)
{
	WRTPSG(4, 0);
	WRTPSG(5, 0);
	WRTPSG(7, static_cast<uint8_t>((RDPSG(7) & 0xc0) | 0x36));
	uint16_t entry = static_cast<uint16_t>(SpeechTable + sample * 4);
	uint16_t hl = static_cast<uint16_t>(M16(entry) + 0x3d00);
	uint16_t de = static_cast<uint16_t>(M16(static_cast<uint16_t>(entry + 2)) + 0x3d00);
	PsgSelect(10);
	DI();
	M(SpeechLevel) = 0x0f;
	do {
		uint8_t b = M(hl);
		Cycles(34);
		PsgData(M(SpeechLevel));
		M(SpeechLevel) ^= 0x0f;
		uint32_t n = b == 0 ? 256 : b;
		uint32_t rest = 22 + 49 * n - 5 + 13;
		if (M(hl) == 0) {
			M(SpeechLevel) ^= 0x0f;
			rest += 31;
		}
		hl++;
		Cycles(rest + 67);
	} while (hl < de);
	PsgData(0);
	EI();
}

}	// namespace


void
Say(uint8_t sample)
{
	Cost(timing::SAY, 0x87e3);
	uint16_t saved = SpeechSavedPsg;
	M(saved) = RDPSG(0);
	M(static_cast<uint16_t>(saved + 1)) = RDPSG(1);
	M(static_cast<uint16_t>(saved + 2)) = RDPSG(7);
	M(static_cast<uint16_t>(saved + 3)) = RDPSG(8);
	WRTPSG(8, 0);
	PlaySample(sample);
	WaitCountDownDE(0x1000);	// SpeechPause
	WRTPSG(0, M(saved));
	WRTPSG(1, M(static_cast<uint16_t>(saved + 1)));
	WRTPSG(7, M(static_cast<uint16_t>(saved + 2)));
	WRTPSG(8, M(static_cast<uint16_t>(saved + 3)));
}
