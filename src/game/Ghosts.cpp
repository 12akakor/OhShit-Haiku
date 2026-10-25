/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The four ghosts (#8DDC-#9601).
//
// Per ghost 4 bytes in Ghosts (y, x, pattern, colour) and one byte (or
// word) in the tables after it. GhostState: 0 normal (blue while the
// player is frightening them), 1 eyes on the way home, #FF back from home
// (no longer blue, deadly). Directions as for the player: 1 up, 2 right,
// 4 down, 8 left.
#include "Game.h"
#include "Timing.h"


using namespace sym;


namespace {

inline uint16_t
G(int ghost)
{
	return static_cast<uint16_t>(Ghosts + ghost * 4);
}


inline uint8_t&
At(uint16_t table, int ghost)
{
	return M(static_cast<uint16_t>(table + ghost));
}


inline uint8_t
LevelIndex()
{
	return M(Level) < 0x0f ? M(Level) : 0x0f;
}


// #8F05: the pattern, unless the ghost is blue.
void
SetHousePattern(int ghost, uint8_t pattern)
{
	if (M(Frightened) != 0 && At(GhostState, ghost) == 0)
		return;
	M(static_cast<uint16_t>(G(ghost) + 2)) = pattern;
}


void
UpdateGhostInHouse(int ghost)
{
	Cost(timing::UPDATE_GHOST_IN_HOUSE, 0x8ebe);
	uint16_t g = G(ghost);
	if (At(GhostLeaving, ghost) != 0) {
		uint16_t x = static_cast<uint16_t>(g + 1);
		if (M(x) != 0x64) {
			if (M(x) > 0x64)
				M(x)--;
			else
				M(x)++;
			return;
		}
		if (M(Frightened) == 0 || At(GhostState, ghost) != 0)
			M(static_cast<uint16_t>(g + 2)) = 0x34;
		if (++At(GhostEyesTimer, ghost) != 0)
			return;
		At(GhostEyesTimer, ghost) = 0xfe;
		if (M(g) == 0x43)
			At(GhostInHouse, ghost) = 0;
		else
			M(g)--;
		return;
	}
	uint16_t timer = static_cast<uint16_t>(GhostHouseTimer + ghost * 2);
	uint16_t t = static_cast<uint16_t>(M16(timer) + 1);
	SetM16(timer, t);
	if (t == 0) {
		At(GhostEyesTimer, ghost) = 0xff;
		At(GhostLeaving, ghost) = 1;
		uint8_t pattern = M(static_cast<uint16_t>(g + 1)) < 0x64 ? 0x2c : 0x3c;
		if (M(Frightened) != 0)
			return;
		M(static_cast<uint16_t>(g + 2)) = pattern;
		return;
	}
	if (At(GhostHouseDir, ghost) == 0) {
		if (M(g) == 0x53) {
			At(GhostHouseDir, ghost) = 1;
			SetHousePattern(ghost, 0x24);
		} else
			M(g)--;
	} else {
		if (M(g) >= 0x5b) {
			At(GhostHouseDir, ghost) = 0;
			SetHousePattern(ghost, 0x34);
		} else
			M(g)++;
	}
}


void
GhostCentre(int ghost)
{
	Cost(timing::GHOST_CENTRE, 0x8f9e);
	uint16_t g = G(ghost);
	M(GhostCentrePos) = static_cast<uint8_t>(M(g) + 9);
	M(static_cast<uint16_t>(GhostCentrePos + 1))
		= static_cast<uint8_t>(M(static_cast<uint16_t>(g + 1)) + 8);
}


// NZ of the original: 'dir' is the reverse of the current direction.
bool
IsReverse(int ghost, uint8_t dir)
{
	Cost(timing::GHOST_REVERSE_CHECK, 0x8fb9);
	uint8_t a = At(GhostDir, ghost);
	if (a & 0x01)
		return dir & 0x04;
	if (a & 0x02)
		return dir & 0x08;
	if (a & 0x04)
		return dir & 0x01;
	return dir & 0x02;
}


// Carry of IsWall: the cell at (de) blocks.
bool
IsWall(uint16_t de)
{
	Cost(timing::IS_WALL, 0x9461);
	uint8_t a = VdpPeek(PosToName(de));
	if (a >= 0x30)
		return false;
	return !(a == 0 || a == 0x01 || a == 0x20);
}


// The four tries (#92D6 right, #9320 left, #9331 up, #938F down): true
// (carry) when the ghost cannot go that way.
bool
BlockedHorizontal(int ghost)
{
	if (IsWall(GhostCentrePos))
		return true;
	return (M(G(ghost)) & 7) != 3;
}


bool
BlockedVertical(int ghost)
{
	if (IsWall(GhostCentrePos))
		return true;
	return (M(static_cast<uint16_t>(G(ghost) + 1)) & 7) != 3;
}


bool
TryRight(int ghost)
{
	Cost(timing::TRY_RIGHT, 0x92d6);
	if (IsReverse(ghost, 0x02))
		return true;
	uint16_t g = G(ghost);
	if (M(g) == 0x73) {
		uint8_t x = M(static_cast<uint16_t>(g + 1));
		if (x >= 0x30 && x < 0x40 && ghost == 3)
			return true;
	}
	GhostCentre(ghost);
	M(static_cast<uint16_t>(GhostCentrePos + 1)) += 5;
	return BlockedHorizontal(ghost);
}


bool
TryLeft(int ghost)
{
	Cost(timing::TRY_LEFT, 0x9320);
	if (IsReverse(ghost, 0x08))
		return true;
	GhostCentre(ghost);
	M(static_cast<uint16_t>(GhostCentrePos + 1)) -= 4;
	return BlockedHorizontal(ghost);
}


bool
TryUp(int ghost)
{
	Cost(timing::TRY_UP, 0x9331);
	if (IsReverse(ghost, 0x01))
		return true;
	uint16_t g = G(ghost);
	uint8_t y = M(g);
	uint8_t x = M(static_cast<uint16_t>(g + 1));
	if (y == 0x43 && x >= 0x54 && x < 0x74)
		return true;
	if (y == 0x93 && (x < 0x28 || x >= 0xa0) && ghost >= 2)
		return true;
	if (x >= 0xc8)
		return true;
	GhostCentre(ghost);
	M(GhostCentrePos) -= 5;
	return BlockedVertical(ghost);
}


bool
TryDown(int ghost)
{
	Cost(timing::TRY_DOWN, 0x938f);
	uint16_t g = G(ghost);
	uint8_t x = M(static_cast<uint16_t>(g + 1));
	if (x >= 0xc8)
		return true;
	if (M(g) == 0x1b && x >= 0x40 && x < 0x88 && (ghost == 1 || ghost == 3))
		return true;
	if (IsReverse(ghost, 0x04))
		return true;
	GhostCentre(ghost);
	M(GhostCentrePos) += 4;
	return BlockedVertical(ghost);
}


// #9062
void
SetGhostDir(int ghost, uint8_t dir)
{
	At(GhostDir, ghost) = dir;
	uint8_t state = At(GhostState, ghost);
	uint16_t table;
	if (state == 0x01)
		table = EyesPatternByDir;
	else if (state == 0xff)
		table = GhostPatternByDir;
	else {
		if (M(Frightened) != 0)
			return;
		table = GhostPatternByDir;
	}
	M(static_cast<uint16_t>(G(ghost) + 2)) = M(static_cast<uint16_t>(table + dir));
}


typedef bool (*Try)(int);

// Tries three directions in order; the fourth is taken when all fail.
void
Choose(int ghost, Try t1, uint8_t d1, Try t2, uint8_t d2, Try t3, uint8_t d3,
	uint8_t d4)
{
	if (!t1(ghost))
		SetGhostDir(ghost, d1);
	else if (!t2(ghost))
		SetGhostDir(ghost, d2);
	else if (!t3(ghost))
		SetGhostDir(ghost, d3);
	else
		SetGhostDir(ghost, d4);
}


// #900A (#9002 from GhostChase, which first checks for eyes)
void
SteerGhost(int ghost, bool fromChase)
{
	if (++At(GhostMoveTimer, ghost) != 0) {
		Cost(fromChase ? 92 : 47, 0x9002);
		return;
	}
	Cost(fromChase ? 720 : 693, 0x9002);
	uint16_t g = G(ghost);
	uint8_t y = M(g);
	uint8_t x = M(static_cast<uint16_t>(g + 1));
	uint8_t targetY = M(ChaseTarget);
	uint8_t targetX = M(ChaseTargetX);
	uint8_t dy = static_cast<uint8_t>(targetY - y);
	if (dy & 0x80)
		dy = static_cast<uint8_t>(-dy);
	uint8_t dx = static_cast<uint8_t>(targetX - x);
	if (dx & 0x80)
		dx = static_cast<uint8_t>(-dx);
	uint8_t dir = At(GhostDir, ghost);
	if (dx < dy) {
		// mostly vertical
		if (targetY >= y) {
			if (dir & 0x02)
				Choose(ghost, TryDown, 4, TryRight, 2, TryUp, 1, 8);
			else if (dir & 0x08)
				Choose(ghost, TryDown, 4, TryLeft, 8, TryUp, 1, 2);
			else if (targetX < x)
				Choose(ghost, TryLeft, 8, TryDown, 4, TryRight, 2, 1);
			else {
				if (targetX == x && !TryDown(ghost)) {
					SetGhostDir(ghost, 4);
					return;
				}
				// #905A: only two more tries after this one
				if (!TryRight(ghost))
					SetGhostDir(ghost, 2);
				else if (!TryDown(ghost))
					SetGhostDir(ghost, 4);
				else if (!TryLeft(ghost))
					SetGhostDir(ghost, 8);
				else
					SetGhostDir(ghost, 1);
			}
		} else {
			if (dir & 0x02)
				Choose(ghost, TryUp, 1, TryRight, 2, TryDown, 4, 8);
			else if (dir & 0x08)
				Choose(ghost, TryUp, 1, TryLeft, 8, TryDown, 4, 2);
			else if (targetX < x)
				Choose(ghost, TryLeft, 8, TryUp, 1, TryRight, 2, 4);
			else {
				if (targetX == x && !TryUp(ghost)) {
					SetGhostDir(ghost, 1);
					return;
				}
				Choose(ghost, TryRight, 2, TryUp, 1, TryLeft, 8, 4);
			}
		}
	} else {
		// mostly horizontal
		if (targetX >= x) {
			if (dir & 0x01)
				Choose(ghost, TryRight, 2, TryUp, 1, TryLeft, 8, 4);
			else if (dir & 0x04)
				Choose(ghost, TryRight, 2, TryDown, 4, TryLeft, 8, 1);
			else if (targetY < y)
				Choose(ghost, TryUp, 1, TryRight, 2, TryDown, 4, 8);
			else {
				if (targetY == y && !TryRight(ghost)) {
					SetGhostDir(ghost, 2);
					return;
				}
				Choose(ghost, TryDown, 4, TryRight, 2, TryUp, 1, 8);
			}
		} else {
			if (dir & 0x01)
				Choose(ghost, TryLeft, 8, TryUp, 1, TryRight, 2, 4);
			else if (dir & 0x04)
				Choose(ghost, TryLeft, 8, TryDown, 4, TryRight, 2, 1);
			else if (targetY < y)
				Choose(ghost, TryUp, 1, TryLeft, 8, TryDown, 4, 2);
			else {
				if (targetY == y && !TryLeft(ghost)) {
					SetGhostDir(ghost, 8);
					return;
				}
				Choose(ghost, TryDown, 4, TryLeft, 8, TryUp, 1, 2);
			}
		}
	}
}


// #8FD6
void
GhostChase(int ghost)
{
	if (GhostScorePause()) {
		Cost(29, 0x8fd6);
		return;
	}
	if (M(Frightened) == 0)
		Cost(160, 0x8fd6);
	else
		Cost(At(GhostState, ghost) == 0 ? 266 : 204, 0x8fd6);
	M(ChaseTarget) = M(Player);
	M(ChaseTargetX) = M(PlayerX);
	if (M(Frightened) != 0 && At(GhostState, ghost) == 0) {
		// Blue ghosts run away from the mirrored position. The original
		// also negates the byte after the target.
		const uint16_t kAfterTarget = 0x8dd8;
		M(kAfterTarget) = static_cast<uint8_t>(-M(kAfterTarget));
		M(ChaseTargetX) = static_cast<uint8_t>(0xc0 - M(ChaseTargetX));
	}
	if (At(GhostState, ghost) == 0x01) {
		Cost(51, 0x9002);
		return;
	}
	SteerGhost(ghost, true);
}


// #93CC
void
MoveGhost(int ghost)
{
	if (At(GhostMoveTimer, ghost) != 0) {
		Cost(48, 0x93cc);
		return;
	}
	Cost(650, 0x93cc);
	uint8_t speed = M(static_cast<uint16_t>(GhostSpeedByLevel + LevelIndex()));
	uint16_t g = G(ghost);
	if (M(g) == 0x5b) {
		uint8_t x = M(static_cast<uint16_t>(g + 1));
		if (x < 0x30 || x >= 0xa0)
			speed = static_cast<uint8_t>(speed - 6);
	}
	At(GhostMoveTimer, ghost) = speed;
	uint8_t state = At(GhostState, ghost);
	bool skip = false;
	if (M(Frightened) != 0) {
		if (state == 0xff)
			skip = true;
		else {
			At(GhostMoveTimer, ghost) = static_cast<uint8_t>(At(GhostMoveTimer, ghost)
				- M(static_cast<uint16_t>(FrightSlowByLevel + LevelIndex())));
		}
	}
	if (!skip && state == 0x01)
		At(GhostMoveTimer, ghost) = 0xfd;
	uint8_t dir = At(GhostDir, ghost);
	uint16_t x = static_cast<uint16_t>(g + 1);
	if (dir & 0x01)
		M(g)--;
	else if (dir & 0x02) {
		if (++M(x) == 0)
			M(x) = 0x08;
	} else if (dir & 0x04)
		M(g)++;
	else {
		if (--M(x) == 0)
			M(x) = 0xf8;
	}
}


// #9536: eyes head for the door of the house.
void
UpdateEyes(int ghost)
{
	if (At(GhostState, ghost) != 0x01) {
		Cost(51, 0x9536);
		return;
	}
	Cost(299, 0x9536);
	uint16_t saved = M16(ChaseTarget);
	SetM16(ChaseTarget, 0x6443);
	SteerGhost(ghost, false);
	MoveGhost(ghost);
	SetM16(ChaseTarget, saved);
	uint16_t g = G(ghost);
	if (M(g) != 0x43 || M(static_cast<uint16_t>(g + 1)) != 0x64)
		return;
	M(g) = 0x53;
	M(static_cast<uint16_t>(g + 3)) = M(static_cast<uint16_t>(GhostStart + ghost * 4 + 3));
	At(GhostState, ghost) = 0xff;
	At(GhostInHouse, ghost) = 1;
	At(GhostLeaving, ghost) = 0;
	SetM16(static_cast<uint16_t>(GhostHouseTimer + ghost * 2), 0xfff8);
	At(GhostEyesTimer, ghost) = 0xff;
	At(GhostDir, ghost) = 0x08;
	for (int i = 0; i < 4; i++) {
		if (At(GhostState, i) == 0x01)
			return;
	}
	EyesSoundOff();
}


// #95E5. The original tests the bits of ReverseGhosts itself, not of the
// direction, so a reverse always gives the same direction.
void
GhostReverse(int ghost)
{
	Cost(timing::GHOST_REVERSE, 0x95e5);
	uint8_t a = M(ReverseGhosts);
	if (a == 0)
		return;
	if (a & 0x01)
		a ^= 0x05;
	else if (a & 0x02)
		a ^= 0x0a;
	else if (a & 0x04)
		a ^= 0x05;
	else
		a ^= 0x0a;
	At(GhostDir, ghost) = a;
}


// #9479
void
UpdateFright()
{
	if (M(Frightened) == 0) {
		Cost(31, 0x9479);
		return;
	}
	Cost(M(FrightTimer) >= 0xc0 ? 530 : 590, 0x9479);
	if (++M(FrightTick) == 0) {
		M(FrightTick) = 0xf0;
		if (++M(FrightTimer) == 0) {
			for (int i = 0; i < 4; i++) {
				M(static_cast<uint16_t>(G(i) + 3))
					= M(static_cast<uint16_t>(GhostStart + i * 4 + 3));
			}
			M(Frightened) = 0;
			FrightSoundOff();
			return;
		}
	}
	if (M(FrightTimer) >= 0xc0) {
		// flashing near the end
		for (int i = 0; i < 4; i++) {
			uint8_t state = At(GhostState, i);
			if (state == 0x01 || state == 0xff)
				continue;
			M(static_cast<uint16_t>(G(i) + 3)) ^= 0x0b;
		}
		return;
	}
	for (int i = 0; i < 4; i++) {
		uint8_t state = At(GhostState, i);
		if (state == 0xff)
			continue;
		M(static_cast<uint16_t>(G(i) + 3)) = state == 0x01 ? 0x0f : 0x04;
	}
}

}	// namespace


void
InitGhosts()
{
	Cost(2236, 0x8ddc);
	M(GhostAnimTimer) = 0xff;
	M(FrightTick) = 0xff;
	M(ScorePause) = 0;
	for (int i = 0; i < 16; i++)
		M(static_cast<uint16_t>(Ghosts + i)) = M(static_cast<uint16_t>(GhostStart + i));
	for (int i = 0; i < 4; i++)
		At(GhostInHouse, i) = At(GhostStartInHouse, i);
	for (int i = 0; i < 8; i++)
		M(static_cast<uint16_t>(GhostHouseTimer + i))
			= M(static_cast<uint16_t>(GhostStartHouseTimer + i));
	for (int i = 0; i < 4; i++) {
		At(GhostLeaving, i) = 0;
		At(GhostHouseDir, i) = At(GhostStartHouseDir, i);
		At(GhostDir, i) = At(GhostStartDir, i);
		At(GhostMoveTimer, i) = 0xff;
		At(GhostHouseTick, i) = 0xff;
		At(GhostState, i) = 0;
	}
	CopyGhostSprites();
}


void
CopyGhostSprites()
{
	for (int i = 0; i < 4; i++) {
		uint16_t from = G(i);
		uint16_t to = static_cast<uint16_t>(SpriteGhosts + i * 4);
		if (M(static_cast<uint16_t>(from + 1)) >= 0xc0) {
			M(to) = 0xd2;
			continue;
		}
		for (int k = 0; k < 4; k++)
			M(static_cast<uint16_t>(to + k)) = M(static_cast<uint16_t>(from + k));
	}
}


void
HideGhosts()
{
	Cost(958, 0x8e78);
	for (int i = 0; i < 4; i++)
		M(G(i)) = 0xd2;
	CopyGhostSprites();
}


void
UpdateGhosts()
{
	Cost(timing::UPDATE_GHOSTS, 0x8e89);
	for (int c = 0; c < 4; c++) {
		if (At(GhostInHouse, c) != 0) {
			if (++At(GhostHouseTick, c) == 0) {
				At(GhostHouseTick, c) = 0xf6;
				UpdateGhostInHouse(c);
			}
			continue;
		}
		UpdateEyes(c);
		GhostChase(c);
		MoveGhost(c);
		GhostReverse(c);
	}
	M(ReverseGhosts) = 0;
	UpdateFright();
	CopyGhostSprites();
}


void
AnimateGhosts()
{
	Cost(timing::ANIMATE_GHOSTS, 0x94f2);
	if (++M(GhostAnimTimer) != 0)
		return;
	M(GhostAnimTimer) = 0xfc;
	for (int i = 0; i < 4; i++) {
		if (At(GhostState, i) == 0x01)
			continue;
		uint16_t pattern = static_cast<uint16_t>(G(i) + 2);
		uint8_t a = M(pattern);
		if (a < 0x2c)
			a ^= 0x0c;
		else if (a < 0x34)
			a ^= 0x1c;
		else if (a < 0x3c)
			a ^= 0x0c;
		else if (a < 0x80)
			a ^= 0x7c;
		else
			a ^= 0x04;
		M(pattern) = a;
	}
	CopyGhostSprites();
}
