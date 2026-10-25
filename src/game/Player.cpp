/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The player (#9B6E-#9F34).
//
// Player holds the sprite (y, x, pattern, colour); PlayerCentre the cell
// position used for walls and dots (y + 9, x + 8). Directions are bits:
// 1 up, 2 right, 4 down, 8 left.
#include "Game.h"
#include "Timing.h"


using namespace sym;


namespace {

void
SetDirection(uint8_t dir, uint16_t anim)
{
	M(PlayerDir) = dir;
	SetM16(PlayerAnimPtr, anim);
}


// Z of the original: the cell at PlayerCentre can be entered (a dot, a
// pill, empty, or a character from #30 on).
bool
CanMove()
{
	Cost(timing::CAN_MOVE, 0x9d21);
	uint8_t a = VdpPeek(PosToName(PlayerCentre));
	if (a == 0 || a == 0x20 || a == 0x01)
		return true;
	return a >= 0x30;
}


void
AfterEating()
{
	AddScore();
	M(DotsEaten)++;
	M(PlayerMoveTimer)--;
}


void
EatDot(uint16_t cell)
{
	Cost(timing::EAT_DOT, 0x9d3a);
	VdpPoke(cell, 0x20);
	M(ScoreTens)++;
	PlayChomp();
	AfterEating();
}


void
EatPill(uint16_t cell)
{
	Cost(400, 0x9d5b);
	VdpPoke(cell, 0x20);
	M(ScoreTens) = static_cast<uint8_t>(M(ScoreTens) + 5);
	M(Frightened) = 1;
	uint8_t level = M(Level) < 0x0f ? M(Level) : 0x0f;
	M(FrightTimer) = M(static_cast<uint16_t>(FrightTimeByLevel + level));
	for (int i = 0; i < 4; i++) {
		uint16_t state = static_cast<uint16_t>(GhostState + i);
		if (M(state) != 0x01)
			M(state) = 0;
	}
	for (int i = 0; i < 4; i++) {
		if (M(static_cast<uint16_t>(GhostState + i)) == 0)
			M(static_cast<uint16_t>(GhostPattern + i * 4)) = 0x90;
	}
	M(GhostsEaten) = 0;
	M(ReverseGhosts) = 1;
	FrightSoundOn();
	AfterEating();
}


void
MovePlayer()
{
	if (++M(PlayerMoveTimer) != 0) {
		Cost(35, 0x9c87);
		return;
	}
	Cost(605, 0x9c87);
	uint8_t level = M(Level) < 0x0f ? M(Level) : 0x0f;
	M(PlayerMoveTimer) = M(static_cast<uint16_t>(PlayerSpeedByLevel + level));
	SetPlayerCentre();
	uint16_t cell = PosToName(PlayerCentre);
	uint8_t a = VdpPeek(cell);
	if (a == 0)
		EatDot(cell);
	if (a == 1)
		EatPill(cell);
	uint8_t dir = M(PlayerDir);
	if (dir & 0x01) {
		M(PlayerCentre) = static_cast<uint8_t>(M(PlayerCentre) - 5);
		if (!CanMove())
			return;
		M(Player)--;
	} else if (dir & 0x02) {
		M(PlayerCentreX) = static_cast<uint8_t>(M(PlayerCentreX) + 5);
		if (!CanMove())
			return;
		M(PlayerX)++;
	} else if (dir & 0x04) {
		M(PlayerCentre) = static_cast<uint8_t>(M(PlayerCentre) + 4);
		if (!CanMove())
			return;
		M(Player)++;
	} else {
		M(PlayerCentreX) = static_cast<uint8_t>(M(PlayerCentreX) - 4);
		if (!CanMove())
			return;
		M(PlayerX)--;
	}
	SetPlayerSprite();
}


// #9BE6: try to turn to 'dir'.
void
Steer(uint8_t dir)
{
	if (dir == 0)
		return;
	SetPlayerCentre();
	if (dir & 0x01) {
		M(PlayerCentre) = static_cast<uint8_t>(M(PlayerCentre) - 8);
		uint8_t x = M(PlayerX);
		if (x >= 0xc0 || x < 0x08 || (x & 7) != 3)
			return;
		if (CanMove())
			SetDirection(1, AnimUp);
	} else if (dir & 0x02) {
		M(PlayerCentreX) = static_cast<uint8_t>(M(PlayerCentreX) + 8);
		if ((M(Player) & 7) != 3)
			return;
		if (CanMove())
			SetDirection(2, AnimRight);
	} else if (dir & 0x04) {
		M(PlayerCentre) = static_cast<uint8_t>(M(PlayerCentre) + 8);
		uint8_t x = M(PlayerX);
		if (x >= 0xc0 || x < 0x08 || (x & 7) != 3)
			return;
		if (CanMove())
			SetDirection(4, AnimDown);
	} else {
		M(PlayerCentreX) = static_cast<uint8_t>(M(PlayerCentreX) - 8);
		if ((M(Player) & 7) != 3)
			return;
		if (CanMove())
			SetDirection(8, AnimLeft);
	}
}


uint8_t
DemoInput()
{
	Cost(40, 0x9f00);
	for (;;) {
		uint8_t e = Random() & 0x03;
		uint8_t candidate = M(static_cast<uint16_t>(DemoDirs + e));
		uint8_t dir = M(PlayerDir);
		uint8_t reverse;
		if (dir == 0x01)
			reverse = 0x04;
		else if (dir == 0x02)
			reverse = 0x08;
		else if (dir == 0x04)
			reverse = 0x01;
		else
			reverse = 0x02;
		if (candidate != reverse)
			return candidate;
	}
}


void
ShowGhostScoreSprite()
{
	for (int i = 0; i < 4; i++) {
		M(static_cast<uint16_t>(SpriteGhostScore + i))
			= M(static_cast<uint16_t>(GhostScoreSprite + i));
	}
}


void
GameOver()
{
	ClearScreen();
	StopInterrupt();
	VdpWrite(TxtGameOver, 0x19ab, 0x0009);
	Say(1);
	EnterHiscore();
	throw ::Restart{ sym::code::Title };
}


void
PlayerDies()
{
	Cost(2000, 0x9e12);
	M(Dying) = 1;
	SilencePsg();
	Say(0);
	UpdateFruit();
	HideGhosts();
	for (int c = 8; c > 0; c--) {
		M(PlayerPattern) = M(static_cast<uint16_t>(DeathAnim + c));
		SetPlayerSprite();
		WaitCountUp(0xc000);
	}
	WaitCountUp(0x0000);
	if (M(DemoMode) != 0)
		throw ::Restart{ sym::code::Title };
	if (--M(Lives) == 0)
		GameOver();
	DrawLives();
	M(Dying) = 0;
	InitGhosts();
	InitPlayer();
	ShowReady();
	StartSiren();
}


void
EatGhost(int ghost)
{
	Cost(600, 0x9e8b);
	uint16_t state = static_cast<uint16_t>(GhostState + ghost);
	if (M(state) == 0xff) {
		PlayerDies();
		return;
	}
	if (M(state) == 0x01)
		return;
	M(state) = 1;
	M(ScorePause) = 1;
	SetM16(ScorePauseTimer, 0xfc00);
	M(GhostScoreSprite) = M(Player);
	M(static_cast<uint16_t>(GhostScoreSprite + 1)) = M(PlayerX);
	M(SavedPlayerY) = M(Player);
	M(Player) = 0xd2;
	SetPlayerSprite();
	uint8_t e = M(GhostsEaten)++;
	M(ScoreHundreds) = static_cast<uint8_t>(M(ScoreHundreds)
		+ M(static_cast<uint16_t>(GhostPoints + e)));
	AddScore();
	M(GhostScorePattern) = M(static_cast<uint16_t>(GhostScorePatterns + e));
	if (M(GhostsEaten) == 4)
		FrightSoundOff();
	PlayEatGhostSound();
	EyesSoundOn();
	ShowGhostScoreSprite();
}

}	// namespace


void
InitPlayer()
{
	Cost(495, 0x9b6e);
	M(PlayerStepTimer) = 0xff;
	M(PlayerMoveTimer) = 0xff;
	M(PlayerAnimTimer) = 0xff;
	M(PlayerDir) = 0x08;
	M(PlayerAnimStep) = 0;
	M(ReverseGhosts) = 0;
	M(Frightened) = 0;
	M(GhostsEaten) = 0;
	M(PlayerUnused2) = 4;
	M(PlayerUnused1) = 4;
	SetM16(PlayerAnimPtr, AnimLeft);
	for (int i = 0; i < 4; i++)
		M(static_cast<uint16_t>(Player + i)) = M(static_cast<uint16_t>(PlayerStart + i));
	SetPlayerSprite();
}


void
SetPlayerSprite()
{
	Cost(timing::SET_PLAYER_SPRITE, 0x9ba4);
	uint8_t x = M(PlayerX);
	if (x >= 0x08 && x < 0xc0) {
		for (int i = 0; i < 4; i++)
			M(static_cast<uint16_t>(SpritePlayer + i)) = M(static_cast<uint16_t>(Player + i));
		return;
	}
	uint8_t y = M(Player);
	M(Player) = 0xd2;
	for (int i = 0; i < 4; i++)
		M(static_cast<uint16_t>(SpritePlayer + i)) = M(static_cast<uint16_t>(Player + i));
	M(Player) = y;
}


void
SetPlayerCentre()
{
	Cost(timing::SET_PLAYER_CENTRE, 0x9db7);
	M(PlayerCentre) = static_cast<uint8_t>(M(Player) + 9);
	M(PlayerCentreX) = static_cast<uint8_t>(M(PlayerX) + 8);
}


// #95BE. True while the score of an eaten ghost shows: the caller returns
// at once (the original pops its return address).
bool
GhostScorePause()
{
	if (M(ScorePause) == 0) {
		Cost(31, 0x95be);
		return false;
	}
	Cost(100, 0x95be);
	uint16_t timer = static_cast<uint16_t>(M16(ScorePauseTimer) + 1);
	SetM16(ScorePauseTimer, timer);
	if (timer != 0)
		return true;
	M(Player) = M(SavedPlayerY);
	M(ScorePause) = 0;
	M(GhostScoreSprite) = 0xd2;
	ShowGhostScoreSprite();
	return false;
}


void
UpdatePlayer()
{
	if (GhostScorePause()) {
		Cost(29, 0x9bcc);
		return;
	}
	MovePlayer();
	CheckGhostCollision();
	if (++M(PlayerStepTimer) != 0) {
		Cost(89, 0x9bcc);
		return;
	}
	M(PlayerStepTimer) = 0xfc;
	uint8_t dir = M(DemoMode) != 0 ? DemoInput() : ReadStick();
	Cost(dir == 0 ? 154 : 390, 0x9bcc);
	Steer(dir);
}


void
AnimatePlayer()
{
	Cost(timing::ANIMATE_PLAYER, 0x9dc8);
	if (++M(PlayerAnimTimer) != 0)
		return;
	M(PlayerAnimTimer) = 0xfe;
	if (M(Dying) != 0)
		return;
	uint8_t step = M(PlayerAnimStep);
	M(PlayerPattern) = M(static_cast<uint16_t>(M16(PlayerAnimPtr) + step));
	SetPlayerSprite();
	step++;
	if (step == 5)
		step = 0;
	M(PlayerAnimStep) = step;
}


void
CheckGhostCollision()
{
	Cost(timing::CHECK_GHOST_COLLISION, 0x9df2);
	for (int b = 4; b > 0; b--) {
		uint16_t ghost = static_cast<uint16_t>(Ghosts + (4 - b) * 4);
		if (!Touches(Player, ghost))
			continue;
		if (M(Frightened) != 0)
			EatGhost(4 - b);
		else
			PlayerDies();
		return;
	}
}
