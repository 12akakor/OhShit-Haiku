/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The joystick test (SELECT on the title screen, #A054-#A1D7): checks that
// the sticks are centred, then shows the direction and fire button of the
// chosen one with sprites. RETURN goes back.
#include "Game.h"

#include "Bios.h"


using namespace sym;


namespace {

const uint16_t kNewKey7 = bios::NEWKEY + 7;


void
Halt()
{
	Cycles(static_cast<uint32_t>(gClock.nextFrame - gClock.cycles));
}


// Waits until only RETURN is down in row 7, then about 0.3 s.
void
WaitKeysUp()
{
	while (M(kNewKey7) != 0x7f)
		Cycles(60);
	Cycles(256 * (12 + 8 + 255 * 14 + 9 + 11 + 14));
}


// True when RETURN is down: back to the game's screen mode.
bool
CheckReturn()
{
	if (M(kNewKey7) != 0x7f)
		return false;
	bios::CLRSPR();
	M(bios::FORCLR) = 0x0f;
	M(bios::BAKCLR) = 0x04;
	M(bios::BDRCLR) = 0x04;
	bios::CHGCLR();
	bios::INITXT();
	bios::KILBUF();
	return true;
}


void
PrintText(uint16_t text)
{
	SetM16(PrintPointer, text);
	for (;;) {
		uint8_t c = M(M16(PrintPointer));
		if (c == '$')
			return;
		bios::CHPUT(c);
		SetM16(PrintPointer, static_cast<uint16_t>(M16(PrintPointer) + 1));
	}
}


void
Light(uint8_t sprite)
{
	uint16_t colour = static_cast<uint16_t>(0x1b00 + ((sprite + 1) * 4 - 1));
	bios::WRTVRM(colour, 0x0f);
}


void
ShowDir(uint8_t dir)
{
	uint16_t entry = static_cast<uint16_t>(JoyTestDirSprites + (dir - 1) * 2);
	Light(M(entry));
	uint8_t second = M(static_cast<uint16_t>(entry + 1));
	if (second != 0xff)
		Light(second);
}

}	// namespace


void
JoystickTest()
{
	M(bios::FORCLR) = 0x0f;
	M(bios::BDRCLR) = 0x04;
	M(bios::BAKCLR) = 0x04;
	bios::CHGCLR();
	bios::INIT32();
	M(bios::RG1SAV) = 0xe2;
	bios::CLRSPR();
	M(bios::RG1SAV) = 0xe2;
	PrintText(TxtJoyTitle);
	PrintText(TxtJoyRelease);
	PrintText(TxtNewLine);
	WaitKeysUp();
	M(JoyTestPort) = 0;
	for (;;) {
		uint8_t port = M(JoyTestPort);
		uint8_t stick = bios::GTSTCK(port);
		PrintText(TxtJoyPort);
		bios::CHPUT(static_cast<uint8_t>(port + 0x30));
		uint16_t result = TxtJoyOk;
		if (stick != 0) {
			result = TxtJoyNotOk;
			M(JoyTestStick) = port ^ 3;
		}
		PrintText(result);
		if (port == 2)
			break;
		M(JoyTestPort) = static_cast<uint8_t>(port + 1);
	}
	uint8_t stick = M(JoyTestStick);
	if (stick == 3 || stick == 0) {
		M(JoyTestStick) = 0;
		PrintText(TxtJoyKeyboard);
	} else
		PrintText(TxtJoyConnect);
	stick = M(JoyTestStick);
	if (stick != 0) {
		bios::CHPUT(static_cast<uint8_t>(stick + 0x30));
		PrintText(TxtJoyPressReturn);
	}
	WaitKeysUp();
	bios::KILBUF();
	bios::CLS();
	PrintText(TxtJoyTitle);
	PrintText(TxtJoyExit);
	bios::LDIRVM(JoyTestPatterns, 0x3800, 0x00a0);
	for (;;) {
		Halt();
		bios::LDIRVM(JoyTestSprites, 0x1b00, 0x0014);
		if (CheckReturn())
			return;
		uint8_t dir = bios::GTSTCK(M(JoyTestStick));
		if (dir != 0)
			ShowDir(dir);
		M(JoyTestPort) = 0x07;
		if (bios::GTTRIG(M(JoyTestStick)) != 0)
			M(JoyTestPort) = 0x06;
		bios::WRTVRM(0x1b13, M(JoyTestPort));
	}
}
