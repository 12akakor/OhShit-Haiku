/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Start, new game, main loop, interrupt and small helpers (#8107-#85AA).
#include "Game.h"
#include "Timing.h"

#include "Bios.h"


using namespace sym;


namespace {

const uint16_t kHookCode = 0x8538;	// "jp Interrupt", copied to H.KEYI


uint8_t
DefaultRefresh()
{
	static uint8_t r;
	return r += 37;
}


void
Init()
{
	Sync(sym::code::Init);
	bios::INIT32();
	M(bios::BDRCLR) = 1;
	SetColours();
	M(bios::RG1SAV) = 0xe2;
	bios::CLRSPR();
	VdpWrite(SpritePatterns, 0x3800, 0x0600);
}


void
NewGame()
{
	Sync(sym::code::NewGame);
	SilencePsg();
	HideSprites();
	VdpWrite(ColourTableGame, 0x2000, 0x0020);
	VdpWrite(MazeChars, 0x0000, 0x0500);
	VdpWrite(SpritePatterns, 0x3800, 0x0600);
	VdpWrite(LivesChars, 0x0010, 0x0020);
	ClearScreen();
	for (int i = 0; i < 6; i++)
		M(static_cast<uint16_t>(Score + i)) = 0;
	M(Level) = 0;
	M(Dying) = 0;
	M(DotsEaten) = 0;
	M(Paused) = 0;
	M(ExtraLifeGiven) = 0;
	M(Lives) = 3;
	SetM16(LoopDelay, 0x0050);
	SetM16(FruitListPos, 0x185a);
	InitPlayer();
	InitGhosts();
	DrawMaze();
	AddScore();
	DrawLives();
	DrawNextFruit();
	ShowReady();
	StartInterrupt();
	StartSiren();
	for (;;) {
		Sync(sym::code::MainLoop);
		Cost(timing::MAIN_LOOP_PASS, 0x823f);
		UpdatePlayer();
		UpdateGhosts();
		UpdateSound();
		CheckEsc();
		CheckPause();
		UpdateFruit();
		CheckLevelDone();
		LoopWait();
		if (M(DemoMode) == 0)
			continue;
		if (ReadTrigger() == 0)
			continue;
		M(DemoMode) = 0;
		throw ::Restart{ sym::code::NewGame };
	}
}

}	// namespace


std::function<uint8_t()> gRefreshValue = DefaultRefresh;


void
Start()
{
	Sync(sym::code::Start);
	EI();
	// halt: wait for the next interrupt
	Cycles(static_cast<uint32_t>(gClock.nextFrame - gClock.cycles));
	DI();
	Run(sym::code::Init);
}


void
Run(uint16_t address)
{
	for (;;) {
		try {
			if (address == sym::code::Init) {
				Init();
				address = sym::code::Restart;
			}
			if (address == sym::code::Restart)
				SilencePsg();
			if (address == sym::code::NewGame)
				NewGame();
			Title();
		} catch (const ::Restart& restart) {
			address = restart.address;
		}
	}
}


void
LoopWait()
{
	uint16_t n = M16(LoopDelay);
	uint32_t count = n == 0 ? 0x10000 : n;
	Cycles(17 + 36 * (count - 1) + 29);
}


void
CheckEsc()
{
	Cost(timing::CHECK_ESC, 0x8277);
	if (bios::SNSMAT(7) & 0x04)
		return;
	throw ::Restart{ sym::code::Restart };
}


uint16_t
PosToName(uint16_t de)
{
	Cost(timing::PosToName(M(de) >> 3), 0x8287);
	uint8_t x = M(de);
	uint8_t y = M(static_cast<uint16_t>(de + 1));
	uint16_t hl = static_cast<uint16_t>(0x1800 | (y >> 3));
	hl = static_cast<uint16_t>(hl + (x >> 3) * 0x20);
	if ((hl >> 8) == 0x1b)
		hl = static_cast<uint16_t>(hl - 0x20);
	return hl;
}


namespace {

// Carry of AbsDiffMinus4: |(de)+7 - ((hl)+7)| < 4.
bool
Near(uint16_t hl, uint16_t de)
{
	uint8_t c = static_cast<uint8_t>(M(hl) + 7);
	uint8_t a = static_cast<uint8_t>(M(de) + 7);
	uint8_t diff = static_cast<uint8_t>(a - c);
	if (a < c)
		diff = static_cast<uint8_t>(-diff);
	return diff < 4;
}

}	// namespace


bool
Touches(uint16_t hl, uint16_t de)
{
	if (!Near(hl, de)) {
		Cost(timing::TOUCHES_ONE, 0x82ac);
		return false;
	}
	Cost(timing::TOUCHES_TWO, 0x82ac);
	return Near(static_cast<uint16_t>(hl + 1), static_cast<uint16_t>(de + 1));
}


void
AddScore()
{
	Cost(timing::ADD_SCORE, 0x82c5);
	if (SkipInDemo())
		return;
	NormaliseScore();
	if (M(static_cast<uint16_t>(Score + 1)) != 1 || M(ExtraLifeGiven) != 0)
		return;
	M(ExtraLifeGiven) = 1;
	M(Lives)++;
	DrawLives();
}


void
NormaliseScore()
{
	Cost(timing::NORMALISE_SCORE, 0x82f0);
	uint16_t hl = ScoreUnits;
	for (int b = 6; b > 0; b--) {
		while (M(hl) >= 10) {
			M(hl) = static_cast<uint8_t>(M(hl) - 10);
			M(static_cast<uint16_t>(hl - 1))++;
		}
		hl--;
	}
	PrintScore();
}


void
PrintScore()
{
	int b = 6;
	uint16_t de = Score;
	uint16_t hl = 0x1808;
	while (M(de) == 0) {
		hl++;
		de++;
		if (--b == 0) {
			b = 1;
			de--;
			hl--;
			break;
		}
	}
	for (; b > 0; b--) {
		VdpPoke(hl, static_cast<uint8_t>(M(de) + 0x30));
		hl++;
		de++;
	}
}


void
DrawLives()
{
	Cost(406, 0x832a);
	if (SkipInDemo())
		return;
	VdpFill(0x1ada, 0x0006, 0x20);
	VdpFill(0x1afa, 0x0006, 0x20);
	uint8_t c = M(Lives);
	if (c == 0)
		return;
	uint16_t hl = 0x1ada;
	for (int b = 4; b > 0; b--) {
		if (--c == 0)
			return;
		uint8_t a = 2;
		VdpPoke(hl, a++);
		hl = static_cast<uint16_t>(hl + 0x20);
		VdpPoke(hl, a++);
		hl = static_cast<uint16_t>(hl - 0x1f);
		VdpPoke(hl, a++);
		hl = static_cast<uint16_t>(hl + 0x20);
		VdpPoke(hl, a);
		hl = static_cast<uint16_t>(hl - 0x1f);
	}
}


void
DrawNextFruit()
{
	Cost(407, 0x8378);
	InitFruit();
	if (M16(FruitListPos) >= 0x1a50) {
		SetM16(FruitListPos, 0x185a);
		uint16_t hl = 0x185a;
		for (int b = 0x12; b > 0; b--) {
			VdpFill(hl, 0x0006, 0x20);
			hl = static_cast<uint16_t>(hl + 0x20);
		}
	}
	uint8_t level = M(Level) < 0x10 ? M(Level) : 0x0f;
	uint8_t a = M(static_cast<uint16_t>(FruitCharByLevel + level));
	uint16_t hl = M16(FruitListPos);
	VdpPoke(hl, a);
	a++;
	hl = static_cast<uint16_t>(hl + 0x20);
	VdpPoke(hl, a);
	hl = static_cast<uint16_t>(hl - 0x20);
	a++;
	hl++;
	VdpPoke(hl, a);
	hl = static_cast<uint16_t>(hl + 0x20);
	a++;
	VdpPoke(hl, a);
	hl = static_cast<uint16_t>(hl - 0x20);
	hl++;
	SetM16(FruitListPos, hl);
	if ((hl & 0x1f) >= 0x10)
		return;
	SetM16(FruitListPos, static_cast<uint16_t>(hl + 0x3a));
}


void
CheckPause()
{
	Cost(timing::CHECK_PAUSE, 0x83e1);
	if (SkipInDemo())
		return;
	if (!KeyP())
		return;
	VdpRead(0x19ca, PauseSavedText, 0x0006);
	M(Paused) = 1;
	VdpWrite(TxtPaused, 0x19ca, 0x0006);
	uint16_t hl = PauseSavedPsg;
	for (uint8_t a = 0x0d; a != 0; a--) {
		M(hl) = bios::RDPSG(a);
		bios::WRTPSG(a, 0);
		hl++;
	}
	do {
		Sync(0x841a);
		Cycles(20);
	} while (KeyP());
	do {
		Sync(0x841f);
		Cycles(20);
	} while (!KeyP());
	do {
		Sync(0x8424);
		Cycles(20);
	} while (KeyP());
	VdpWrite(PauseSavedText, 0x19ca, 0x0006);
	hl = PauseSavedPsg;
	for (uint8_t a = 0x0d; a != 0; a--) {
		bios::WRTPSG(a, M(hl));
		hl++;
	}
	M(Paused) = 0;
}


// True when P is pressed (the original returns Z when it is not).
bool
KeyP()
{
	Cost(timing::KEY_P, 0x8447);
	Cycles(50);
	return (bios::SNSMAT(4) & 0x20) == 0;
}


uint8_t
ReadStick()
{
	Cost(timing::READ_STICK, 0x844f);
	for (int b = 3; b > 0; b--) {
		uint8_t a = bios::GTSTCK(static_cast<uint8_t>(b - 1));
		if (a != 0)
			return M(static_cast<uint16_t>(StickToDir + a));
	}
	return 0;
}


uint8_t
ReadTrigger()
{
	Cost(timing::READ_TRIGGER, 0x8468);
	for (int b = 3; b > 0; b--) {
		uint8_t a = bios::GTTRIG(static_cast<uint8_t>(b - 1));
		if (a != 0)
			return a;
	}
	return 0;
}


namespace {

uint8_t
RandomByte()
{
	Cost(timing::RANDOM_BYTE, 0x84a0);
	// hl = #5000 + bc, with c = R xor JIFFY and b = (-c) and 7
	uint8_t c = static_cast<uint8_t>(gRefreshValue() ^ M(bios::JIFFY));
	uint8_t b = static_cast<uint8_t>(-c) & 7;
	return gPage1[(b << 8) | c];
}

}	// namespace


uint8_t
Random()
{
	Cost(timing::RANDOM, 0x8476);
	uint8_t e = RandomByte();
	// rla: the carry is 0 here (ADD HL,BC in RandomByte cannot overflow)
	uint8_t a = static_cast<uint8_t>((RandomByte() << 1) & 0x1f);
	e = static_cast<uint8_t>(a | e);
	a = static_cast<uint8_t>(((M(bios::JIFFY) & 0x03) | e) ^ M(RandomValue));
	M(RandomValue) = a;
	return a;
}


void
DrawMaze()
{
	Cost(157, 0x84b2);
	VdpWrite(MazeMap, 0x1800, 0x0300);
}


void
ShowReady()
{
	Cost(29, 0x84be);
	if (SkipInDemo())
		return;
	StopInterrupt();
	VdpWrite(TxtReady, 0x19cb, 0x0006);
	M(Paused) = 1;
	for (int b = 0x10; b > 0; b--)
		WaitCountUp(0xa000);
	VdpFill(0x19cb, 0x0006, 0x20);
	M(Paused) = 0;
	StartInterrupt();
}


void
ClearScreen()
{
	Cost(157, 0x84f4);
	VdpFill(0x1800, 0x0300, 0x20);
}


void
StopInterrupt()
{
	Cost(178, 0x851e);
	M(bios::HKEYI) = 0xc9;
	HideSprites();
}


void
StartInterrupt()
{
	Cost(137, 0x8526);
	DI();
	for (int i = 0; i < 3; i++)
		M(static_cast<uint16_t>(bios::HKEYI + i)) = M(static_cast<uint16_t>(kHookCode + i));
	EI();
	M(SpriteRotation) = 0;
}


void
HideSprites()
{
	Cost(145, 0x98fb);
	VdpFill(0x1b00, 0x007f, 0xd2);
}


bool
SkipInDemo()
{
	Cost(timing::SKIP_IN_DEMO, 0x8b9d);
	return M(DemoMode) != 0;
}


void
CheckSelect()
{
	Cost(147, 0x8ba4);
	if (bios::SNSMAT(7) & 0x40)
		return;
	JoystickTest();
	throw ::Restart{ sym::code::Init };
}


void
Interrupt()
{
	Cost(timing::GAME_INTERRUPT, 0x853b);
	M(InInterrupt) = 0xff;
	if (M(Paused) == 0) {
		AnimatePlayer();
		AnimateGhosts();
	}
	uint8_t rotation = M(SpriteRotation);
	uint16_t out = SpriteOut;
	for (int i = rotation * 4; i < 7 * 4; i++)
		M(out++) = M(static_cast<uint16_t>(SpritePlayer + i));
	for (int i = 0; i < rotation * 4; i++)
		M(out++) = M(static_cast<uint16_t>(SpritePlayer + i));
	VdpWrite(SpriteOut, 0x1b00, 0x001c);
	rotation++;
	if (rotation == 6)
		rotation = 0;
	M(SpriteRotation) = rotation;
	M(InInterrupt) = 0;
	M(bios::JIFFY)++;
	VdpIn(1);
}


void
RunInterrupt()
{
	gClock.interruptsEnabled = false;
	if (M(bios::HKEYI) == 0xc3)
		Interrupt();
	else
		bios::Interrupt();
	gClock.interruptsEnabled = true;
}
