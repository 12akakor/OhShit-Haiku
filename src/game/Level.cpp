/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// End of a maze and the intermissions (#964D-#9913).
#include "Game.h"
#include "Timing.h"


using namespace sym;


namespace {

void
ShortWait()
{
	WaitCountUp(0xffc0);
}


void
LongWait()
{
	WaitCountUp(0xa000);
}


void
CopyIntermissionSprites(int count)
{
	uint16_t hl = CutSprites;
	uint16_t de = 0x1b00;
	for (int i = 0; i < count; i++) {
		uint8_t x = M(static_cast<uint16_t>(hl + 1));
		if (x < 0x08 || x >= 0xf8) {
			uint8_t y = M(hl);
			M(hl) = 0xd2;
			VdpWrite(hl, de, 4);
			M(hl) = y;
		} else
			VdpWrite(hl, de, 4);
		hl = static_cast<uint16_t>(hl + 4);
		de = static_cast<uint16_t>(de + 4);
	}
}


void
NextMaze()
{
	HideSprites();
	DrawMaze();
	InitPlayer();
	InitGhosts();
	RedrawFruitList();
	DrawLives();
	ShowReady();
	StartSiren();
}


void
StartIntermission()
{
	M(InInterrupt) = 0xff;
	for (int i = 0; i < 8; i++)
		M(static_cast<uint16_t>(CutSprites + i)) = M(static_cast<uint16_t>(CutStart1 + i));
	M(CutTimerA) = 0xff;
	M(CutTimerB) = 0xff;
	M(CutAnimTimer) = 0xff;
	M(CutAnimStep) = 0;
}


void
NextAnimStep()
{
	uint8_t step = static_cast<uint8_t>(M(CutAnimStep) + 1);
	if (step == 5)
		step = 0;
	M(CutAnimStep) = step;
}


// The player runs from a ghost, then a big player chases the ghosts.
void
Intermission1()
{
	ClearScreen();
	StartIntermission();
	for (uint32_t n = 0x1000; n > 0; n--) {
		Cost(690);	// the loop and CopyIntermissionSprites, without the copies
		if (++M(CutAnimTimer) == 0) {
			M(CutAnimTimer) = 0xfb;
			M(CutSprite1Pattern) ^= 0x7c;
			M(CutSprite0Pattern) = M(static_cast<uint16_t>(CutPlayerAnim + M(CutAnimStep)));
			NextAnimStep();
		}
		// #971C, written with the jumps of the original
		int b = 2;
		uint16_t hl = CutSpritesX;
		if (++M(CutTimerA) != 0)
			goto move1Next;
		M(CutTimerA) = 0xf0;
move1Dec:
		if (--M(hl) == 0)
			M(hl)++;
move1Next:
		hl = static_cast<uint16_t>(hl + 4);
		if (++M(CutTimerB) == 0) {
			M(CutTimerB) = 0xf1;
			if (--b != 0)
				goto move1Dec;
		}
		CopyIntermissionSprites(2);
		ShortWait();
	}
	for (int i = 0; i < 0x14; i++)
		M(static_cast<uint16_t>(CutSprites + i)) = M(static_cast<uint16_t>(CutStart2 + i));
	M(CutTimerA) = 0xff;
	M(CutTimerB) = 0xff;
	for (uint32_t n = 0x1000; n > 0; n--) {
		Cost(1330);
		if (++M(CutAnimTimer) == 0) {
			M(CutAnimTimer) = 0xfb;
			M(CutSprite0Pattern) ^= 0x04;
			uint16_t anim = static_cast<uint16_t>(CutGhostAnim + M(CutAnimStep) * 2);
			M(CutSprite2Pattern) = M(anim);
			M(CutSprite4Pattern) = M(static_cast<uint16_t>(anim + 1));
			NextAnimStep();
		}
		// #979C
		int b = 5;
		uint16_t hl = CutSpritesX;
		M(CutTimerB)++;
		if (++M(CutTimerA) != 0)
			goto move2Next;
		M(CutTimerA) = 0xf0;
move2Inc:
		if (++M(hl) == 0)
			M(hl)--;
move2Next:
		hl = static_cast<uint16_t>(hl + 4);
		if (M(CutTimerB) == 0) {
			if (--b != 0)
				goto move2Inc;
			M(CutTimerB) = 0xf1;
		}
		CopyIntermissionSprites(5);
		ShortWait();
	}
	NextMaze();
}


// The player runs from a ghost that tears its sheet on a nail.
void
Intermission2()
{
	ClearScreen();
	VdpPoke(0x1991, 0x06);
	StartIntermission();
	do {
		Cost(700);
		if (++M(CutAnimTimer) == 0) {
			M(CutAnimTimer) = 0xfb;
			if (M(CutSprite1X) >= 0x80)
				M(CutSprite1Pattern) ^= 0x7c;
			M(CutSprite0Pattern) = M(static_cast<uint16_t>(CutPlayerAnim + M(CutAnimStep)));
			NextAnimStep();
		}
		if (++M(CutTimerA) == 0) {
			M(CutTimerA) = 0xf0;
			if (--M(CutSpritesX) == 0)
				M(CutSpritesX)++;
		}
		if (++M(CutTimerB) == 0) {
			uint8_t x = --M(CutSprite1X);
			if (x >= 0x80)
				M(CutTimerB) = 0xf2;
			else {
				M(CutSprite1Pattern) = M(static_cast<uint16_t>(CutShrinkPatterns + x - 0x78));
				M(CutTimerB) = 0;
			}
		}
		CopyIntermissionSprites(2);
		ShortWait();
	} while (M(CutSprite1X) != 0x78);
	for (int b = 8; b > 0; b--) {
		M(CutSprite1Pattern) ^= 0x0c;
		CopyIntermissionSprites(2);
		LongWait();
	}
	NextMaze();
}

}	// namespace


void
CheckLevelDone()
{
	Cost(timing::CHECK_LEVEL_DONE, 0x964d);
	if (M(DotsEaten) != 0xcd)
		return;
	StopInterrupt();
	SilencePsg();
	uint16_t delay = static_cast<uint16_t>(M16(LoopDelay) - 2);
	if ((delay & 0xff) >= 0x38)
		SetM16(LoopDelay, delay);
	M(DotsEaten) = 0;
	for (int b = 0x10; b > 0; b--) {
		for (uint16_t hl = 0x2001; hl < 0x2003; hl++)
			VdpPoke(hl, VdpPeek(hl) ^ 0xb0);
		WaitCountUp(0xa800);
	}
	VdpPoke(0x2001, 0x41);
	VdpPoke(0x2002, 0x41);
	uint8_t level = ++M(Level);
	if (level == 0x02 || level == 0x09)
		Intermission1();
	else if (level == 0x05 || level == 0x0e)
		Intermission2();
	else
		NextMaze();
}


void
RedrawFruitList()
{
	Cost(604, 0x98a4);
	SetM16(FruitListPos, 0x185a);
	uint8_t level = M(Level);
	M(Level) = 0;
	DrawMaze();
	for (;;) {
		DrawNextFruit();
		if (++M(Level) == level)
			break;
	}
	DrawNextFruit();
}
