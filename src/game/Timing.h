/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_TIMING_H
#define OS_TIMING_H


// CPU cycles of the original routines, so the rewrite runs at the speed of
// the original: the game speed comes from the time a pass of the main loop
// takes (work plus a busy wait), not from the 60 Hz interrupt.
//
// Measured with tools/osprof on the reference machine (MSX1 with C-BIOS):
// the average exclusive cycles of each routine (without the routines it
// calls, which charge their own), over 60000 frames of play. Loops with a
// count (video copies, PosToName) use the exact formula of the original
// code. Each rewritten routine charges its cost when it is entered.

#include <stdint.h>

#include "Machine.h"


namespace timing {

// BIOS (C-BIOS)
const uint32_t WRTPSG = 84;
const uint32_t RDPSG = 46;
const uint32_t SNSMAT = 109;
const uint32_t GTSTCK_KEYBOARD = 192 + 98;
const uint32_t GTSTCK_JOYSTICK = 319 + 35 + 73 + 35;
const uint32_t GTTRIG_SPACE = 90 + 98;
const uint32_t GTTRIG_JOYSTICK = 296 + 35 + 73 + 35;
const uint32_t BIOS_INTERRUPT = 1200;	// C-BIOS handler with H.KEYI = RET
const uint32_t INIT32 = 956 + 40000;
const uint32_t CLRSPR = 151 + 15000;

// The interrupt with the game's handler: C-BIOS pushes, the jump in
// H.KEYI, the handler's own code, the pops (its routines charge their own).
const uint32_t GAME_INTERRUPT = 171 + 11 + 300 + 130;

// Video routines (with WithDi and the address setting).
const uint32_t VDP_PEEK = 102 + 53 + 85 + 127;
const uint32_t VDP_POKE = 102 + 53 + 108 + 135;
inline uint32_t VdpFill(uint32_t n) { return 414 + 67 * n; }
inline uint32_t VdpWrite(uint32_t n) { return 479 + 48 * n; }
inline uint32_t VdpRead(uint32_t n) { return 465 + 48 * n; }
const uint32_t SET_COLOURS = 103 + 53 + 2435;

// Helpers
inline uint32_t PosToName(uint32_t rows) { return 157 + 43 * rows; }
const uint32_t TOUCHES_ONE = 30 + 79;	// first axis already too far
const uint32_t TOUCHES_TWO = 67 + 2 * 79;
const uint32_t SET_PLAYER_CENTRE = 95;
const uint32_t SET_PLAYER_SPRITE = 177;
const uint32_t SKIP_IN_DEMO = 31;
const uint32_t GHOST_SCORE_PAUSE = 31;
const uint32_t GHOST_CENTRE = 169;
const uint32_t GHOST_REVERSE_CHECK = 104;
const uint32_t IS_WALL = 110;
const uint32_t CAN_MOVE = 88;

// Main loop
const uint32_t MAIN_LOOP_PASS = 90;	// the calls and the demo check (fitted)
const uint32_t CHECK_ESC = 48;
const uint32_t CHECK_PAUSE = 48;
const uint32_t KEY_P = 47;
const uint32_t READ_STICK = 223;
const uint32_t READ_TRIGGER = 229;
const uint32_t RANDOM = 280;
const uint32_t RANDOM_BYTE = 100;
const uint32_t CHECK_LEVEL_DONE = 34;

// Player
const uint32_t UPDATE_PLAYER = 148;
const uint32_t MOVE_PLAYER = 102;
const uint32_t CHECK_GHOST_COLLISION = 458;
const uint32_t EAT_DOT = 172;
const uint32_t ANIMATE_PLAYER = 60;

// Ghosts
const uint32_t UPDATE_GHOSTS = 1396;
const uint32_t UPDATE_GHOST_IN_HOUSE = 359;
const uint32_t UPDATE_EYES = 51;
const uint32_t GHOST_CHASE = 160;
const uint32_t GHOST_STEER = 163;
const uint32_t MOVE_GHOST = 97;
const uint32_t GHOST_REVERSE = 31;
const uint32_t UPDATE_FRIGHT = 31;
const uint32_t TRY_RIGHT = 284;
const uint32_t TRY_LEFT = 220;
const uint32_t TRY_UP = 320;
const uint32_t TRY_DOWN = 324;
const uint32_t ANIMATE_GHOSTS = 60;
const uint32_t COPY_GHOST_SPRITES = 400;

// Sound
const uint32_t UPDATE_SOUND = 250;
const uint32_t UPDATE_CHOMP = 45;
const uint32_t PLAY_CHOMP = 332;
const uint32_t SILENCE_PSG = 659;
const uint32_t START_SIREN = 372;
const uint32_t SAY = 482;

// Fruit
const uint32_t UPDATE_FRUIT = 179;
const uint32_t CHECK_EAT_FRUIT = 34;
const uint32_t BLINK_PILLS = 38;
const uint32_t UPDATE_SCATTER = 88;

// Score
const uint32_t ADD_SCORE = 158;
const uint32_t NORMALISE_SCORE = 442 + 313;

}	// namespace timing


// The second form names the original routine, for tools/osprof style
// comparisons (game_diff -profile).
extern void (*gCostHook)(uint32_t cycles, uint16_t routine);

static inline void
Cost(uint32_t cycles)
{
	Cycles(cycles);
}


static inline void
Cost(uint32_t cycles, uint16_t routine)
{
	if (gCostHook != nullptr)
		gCostHook(cycles, routine);
	Cycles(cycles);
}


#endif	// OS_TIMING_H
