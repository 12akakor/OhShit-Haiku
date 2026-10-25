/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The fruit, the blinking pills and the ghosts' turns (#9967-#9AFE).
#include "Game.h"
#include "Timing.h"


using namespace sym;


namespace {

inline uint8_t
FruitLevel()
{
	return M(Level) < 0x10 ? M(Level) : 0x0f;
}


void
CopyFruitSprite()
{
	for (int i = 0; i < 4; i++)
		M(static_cast<uint16_t>(SpriteFruit + i)) = M(static_cast<uint16_t>(FruitSprite + i));
}


void
HideFruit()
{
	M(FruitShown) = 0;
	M(FruitSprite) = 0xd2;
	CopyFruitSprite();
}


void
BlinkPills()
{
	Cost(timing::BLINK_PILLS, 0x9abc);
	if (++M(BlinkTimer) != 0)
		return;
	M(BlinkTimer) = 0xc0;
	uint8_t phase = ++M(BlinkPhase);
	if ((phase & 1) == 0)
		VdpWrite(PillChar, 0x0008, 0x0008);
	else
		VdpFill(0x0008, 0x0008, 0);
}


void
CheckEatFruit()
{
	Cost(timing::CHECK_EAT_FRUIT, 0x9a37);
	if (M(FruitShown) == 0)
		return;
	if (!Touches(Player, FruitSprite))
		return;
	uint8_t level = FruitLevel();
	M(ScoreHundreds) = static_cast<uint8_t>(M(ScoreHundreds)
		+ M(static_cast<uint16_t>(FruitPointsByLevel + level)));
	AddScore();
	uint16_t digits = static_cast<uint16_t>(FruitScoreDigits + level * 2);
	uint16_t hl = 0x19cb;
	if (M(digits) != 0x30)
		VdpPoke(hl, M(digits));
	hl++;
	VdpPoke(hl, M(static_cast<uint16_t>(digits + 1)));
	hl++;
	VdpPoke(hl, 0x30);
	hl++;
	VdpPoke(hl, 0x30);
	SetM16(FruitTimer, 0xfd80);
	M(FruitScoreShown) = 1;
	PlayEatGhostSound();
	HideFruit();
}


void
UpdateScatter()
{
	Cost(timing::UPDATE_SCATTER, 0x9ae4);
	if (M(ReverseGhosts) != 0)
		return;
	uint16_t timer = static_cast<uint16_t>(M16(ScatterTimer) + 1);
	SetM16(ScatterTimer, timer);
	if (timer != 0)
		return;
	SetM16(ScatterTimer, 0xf600);
	M(ReverseGhosts) = 1;
}


// Counts FruitTimer up; true when it reaches 0.
bool
FruitTimerDone()
{
	uint16_t timer = static_cast<uint16_t>(M16(FruitTimer) + 1);
	SetM16(FruitTimer, timer);
	return timer == 0;
}

}	// namespace


void
InitFruit()
{
	Cost(165, 0x9967);
	SetM16(FruitTimer, 0xec00);
	M(FruitCount) = 0;
	M(FruitShown) = 0;
	M(FruitScoreShown) = 0;
	M(BlinkPhase) = 0;
	M(BlinkTimer) = 0xff;
	HideFruit();
	SetM16(ScatterTimer, 0xfd00);
}


void
UpdateFruit()
{
	Cost(timing::UPDATE_FRUIT, 0x9988);
	BlinkPills();
	CheckEatFruit();
	UpdateScatter();
	if (M(FruitScoreShown) != 0) {
		if (M(Dying) == 0 && !FruitTimerDone())
			return;
		SetM16(FruitTimer, 0xea80);
		M(FruitScoreShown) = 0;
		VdpFill(0x19cb, 0x0004, 0x20);
		return;
	}
	if (M(FruitShown) != 0) {
		if (M(Dying) == 0) {
			if (!FruitTimerDone())
				return;
			SetM16(FruitTimer, 0xe800);
		}
		HideFruit();
		return;
	}
	if (!FruitTimerDone())
		return;
	if (M(FruitCount) == 3)
		return;
	M(FruitCount)++;
	M(FruitShown) = 1;
	VdpFill(0x3a20, 0x0020, 0);
	uint8_t level = FruitLevel();
	uint16_t pattern = static_cast<uint16_t>(MazeChars
		+ M(static_cast<uint16_t>(FruitCharByLevel + level)) * 8);
	VdpWrite(pattern, 0x3a20, 0x0020);
	for (int i = 0; i < 4; i++)
		M(static_cast<uint16_t>(FruitSprite + i)) = M(static_cast<uint16_t>(FruitSpriteStart + i));
	M(FruitColour) = M(static_cast<uint16_t>(FruitColourByLevel + level));
	SetM16(FruitTimer, 0xf000);
	CopyFruitSprite();
}
