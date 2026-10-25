/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Title screen, high scores and the character set (#88C7-#8BB6).
#include "Game.h"
#include "Timing.h"

#include "Bios.h"


using namespace sym;


namespace {

const uint16_t kHiscoreScores = 0x8be8;	// score of line 1 (after "1ST JOE ")
const uint16_t kLine4Name = 0x8c0e;
const uint16_t kLine5Name = 0x8c1c;
const uint16_t kAlphabetEnd = 0x8d41;	// one past the space after Z
const uint16_t kSpace = 0x8d40;


void
InputDelay()
{
	WaitCountDownBC(0x2800);
}

}	// namespace


void
PrintLine(uint16_t text, uint16_t vram, uint8_t offset)
{
	Cost(200, 0x8b42);
	for (int b = 0x20; b > 0; b--) {
		VdpPoke(vram, static_cast<uint8_t>(M(text) + offset));
		vram++;
		text++;
	}
}


void
LoadCharset()
{
	Cost(450, 0x8b4f);
	VdpFill(0x0000, 0x07f8, 0);
	M(bios::BDRCLR) = 1;
	SetColours();
	VdpWrite(LetterChars, 0x0208, 0x00e0);
	VdpWrite(LetterChars, 0x0408, 0x00e0);
	VdpWrite(LetterChars, 0x0508, 0x00e0);
	VdpWrite(LetterChars, 0x0608, 0x00e0);
	VdpWrite(LetterChars, 0x0708, 0x00e0);
	VdpWrite(DigitChars, 0x0180, 0x0050);
	VdpWrite(ColourTableTitle, 0x2000, 0x0020);
}


void
DrawHiscores()
{
	Cost(300, 0x88c7);
	HideSprites();
	VdpFill(0x1b00, 0x007f, 0xd2);
	uint16_t hl = 0x1909;
	uint16_t de = HiscoreTable;
	for (int b = 5; b > 0; b--) {
		uint16_t line = hl;
		for (int i = 0; i < 0x0e; i++) {
			VdpPoke(line, M(de));
			de++;
			line++;
		}
		hl = static_cast<uint16_t>(hl + 0x20);
	}
}


void
EnterHiscore()
{
	for (int i = 0; i < 6; i++)
		M(static_cast<uint16_t>(Score + i)) += 0x30;
	// From the last line up: find the first line the score does not beat
	// (an equal score counts as beaten).
	M(HiscorePlace) = 5;
	uint16_t offset = 0x38;
	for (;;) {
		SetM16(HiscoreOffset, offset);
		uint16_t hl = static_cast<uint16_t>(kHiscoreScores + offset);
		uint16_t de = Score;
		bool lower = false;
		for (int b = 6; b > 0; b--) {
			if (M(de) != M(hl)) {
				lower = M(de) < M(hl);
				break;
			}
			hl++;
			de++;
		}
		if (lower) {
			if (M(HiscorePlace) == 5)
				return;
			break;
		}
		if (--M(HiscorePlace) == 0)
			break;
		offset = static_cast<uint16_t>(M16(HiscoreOffset) - 0x0e);
	}

	// Move the lines from the new place on one down (name, space, score).
	uint8_t place = M(HiscorePlace);
	if (place != 4) {
		int count = (place ^ 3) + 1;
		uint16_t from = kLine4Name;
		uint16_t to = kLine5Name;
		for (int i = 0; i < count; i++) {
			for (int k = 0; k < 0x0a; k++)
				M(static_cast<uint16_t>(to + k)) = M(static_cast<uint16_t>(from + k));
			to = static_cast<uint16_t>(to - 0x0e);
			from = static_cast<uint16_t>(from - 0x0e);
		}
	}

	// The score and three spaces for the name.
	uint16_t entry = static_cast<uint16_t>(kHiscoreScores + place * 14);
	for (int i = 0; i < 6; i++)
		M(static_cast<uint16_t>(entry + i)) = M(static_cast<uint16_t>(Score + i));
	uint16_t name = static_cast<uint16_t>(entry - 4);
	M(name) = 0x20;
	M(static_cast<uint16_t>(name + 1)) = 0x20;
	M(static_cast<uint16_t>(name + 2)) = 0x20;

	LoadCharset();
	ClearScreen();
	DrawHiscores();
	PrintLine(TxtOhShit, 0x1a00, 0x80);
	PrintLine(TxtOhShit, 0x18a0, 0x40);
	SetM16(HiscoreNamePos, M16(static_cast<uint16_t>(HiscoreNamePositions + place * 2)));

	uint16_t hl = name;
	for (int b = 3; b > 0; b--) {
		uint16_t de = Alphabet;
		for (;;) {
			Sync(0x89c6);
			uint8_t a = M(de);
			M(hl) = a;
			if (a != 0x20)
				a = static_cast<uint8_t>(a + 0x40);
			VdpPoke(M16(HiscoreNamePos), a);
			InputDelay();
			uint8_t dir = ReadStick();
			if (dir == 0x02) {
				de++;
				if (de >= kAlphabetEnd)
					de = Alphabet;
				continue;
			}
			if (dir == 0x08) {
				if (de - 1 < Alphabet)
					de = kSpace;
				else
					de--;
				continue;
			}
			if (ReadTrigger() == 0)
				continue;
			do {
				Sync(0x8a23);
				Cycles(100);
			} while (ReadTrigger() != 0);
			break;
		}
		SetM16(HiscoreNamePos, static_cast<uint16_t>(M16(HiscoreNamePos) + 1));
		hl++;
	}
	DrawHiscores();
}


void
Title()
{
	Sync(sym::code::Title);
	DI();
	StopInterrupt();
	LoadCharset();
	ClearScreen();
	DrawHiscores();
	PrintLine(TxtWrittenBy, 0x1a20, 0xa0);
	PrintLine(TxtSpeechBy, 0x1a60, 0x40);
	PrintLine(TxtCopyright, 0x1a80, 0x60);
	PrintLine(TxtPressFire, 0x1aa0, 0x80);
	PrintLine(TxtSelect, 0x1ac0, 0x80);
	PrintLine(TxtOhShit, 0x19e0, 0x40);
	PrintLine(TxtGhostNames, 0x1880, 0x60);
	Say(kSpeechOhShit);
	VdpWrite(TitleSpriteJoey, 0x1b00, 0x0004);
	Say(kSpeechJoey);
	VdpWrite(TitleSpritePaul, 0x1b04, 0x0004);
	Say(kSpeechPaul);
	VdpWrite(TitleSpriteWilly, 0x1b08, 0x0004);
	Say(kSpeechWilly);
	Say(kSpeechAnd);
	VdpWrite(TitleSpriteFrankie, 0x1b0c, 0x0004);
	Say(kSpeechFrankie);
	for (int b = 0x80; b > 0; b--) {
		Sync(0x8aee);
		WaitCountDownDE(0x1000);
		if (ReadTrigger() != 0) {
			M(DemoMode) = 0;
			throw ::Restart{ sym::code::NewGame };
		}
		CheckSelect();
		uint16_t hl = 0x1b02;
		for (int k = 4; k > 0; k--) {
			uint8_t a = VdpPeek(hl);
			if (a == 0x24)
				a = 0x30;
			else if (a == 0x30)
				a = 0x34;
			else if (a == 0x34)
				a = 0x40;
			else
				a = 0x24;
			VdpPoke(hl, a);
			hl = static_cast<uint16_t>(hl + 4);
		}
	}
	M(DemoMode) = 1;
	throw ::Restart{ sym::code::NewGame };
}
