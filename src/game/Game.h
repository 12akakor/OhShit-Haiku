/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_GAME_H
#define OS_GAME_H


// The routines of the original program, rewritten in C++. Names follow
// tools/dis/names.txt; every routine does to memory, video memory and the
// PSG what the original does, so it can be compared with it. Routines that
// return a value return what the original leaves in A (or a flag).

#include <stdint.h>

#include <functional>

#include "Machine.h"
#include "Symbols.h"


// Video (#9F3C-#A04E)
void		WithDiEnd();
void		VdpReadAddress(uint16_t hl);
void		VdpWriteAddress(uint16_t hl);
void		VdpFill(uint16_t hl, uint16_t bc, uint8_t a);
void		VdpFillEi(uint16_t hl, uint16_t bc, uint8_t a);
void		VdpPoke(uint16_t hl, uint8_t a);
uint8_t		VdpPeek(uint16_t hl);
void		SetColours();
void		VdpWriteReg(uint8_t reg, uint8_t value);
void		VdpRead(uint16_t vram, uint16_t ram, uint16_t count);
void		VdpWrite(uint16_t ram, uint16_t vram, uint16_t count);

// Busy waits (the cycles of the original loops).
void		WaitCountUp(uint16_t start);	// ld hl,start: inc hl until 0
void		WaitCountDownDE(uint16_t count);	// ld de,count: dec de until 0
void		WaitCountDownBC(uint16_t count);	// ld bc,count: dec bc until 0

// Main program
[[noreturn]] void	Start();
// Runs the program from Init, Restart, NewGame or Title.
[[noreturn]] void	Run(uint16_t address);
void		LoopWait();
void		CheckEsc();
uint16_t	PosToName(uint16_t de);
bool		Touches(uint16_t hl, uint16_t de);
void		AddScore();
void		NormaliseScore();
void		PrintScore();
void		DrawLives();
void		DrawNextFruit();
void		CheckPause();
bool		KeyP();
uint8_t		ReadStick();
uint8_t		ReadTrigger();
uint8_t		Random();
void		DrawMaze();
void		ShowReady();
void		ClearScreen();
void		StopInterrupt();
void		StartInterrupt();
void		Interrupt();
void		HideSprites();
bool		SkipInDemo();
void		CheckSelect();
void		LoadCharset();
void		PrintLine(uint16_t text, uint16_t vram, uint8_t offset);

// Sound and speech
void		SilencePsg();
void		StartSiren();
void		UpdateSound();
void		FrightSoundOn();
void		FrightSoundOff();
void		EyesSoundOn();
void		EyesSoundOff();
void		PlayChomp();
void		PlayEatGhostSound();
void		Say(uint8_t sample);

// The eight speech samples. The names follow where the game plays them
// (not yet checked by listening).
enum {
	kSpeechDying = 0,		// the player is caught
	kSpeechGameOver = 1,
	kSpeechJoey = 2,		// title: the ghost names, one by one
	kSpeechPaul = 3,
	kSpeechWilly = 4,
	kSpeechFrankie = 5,
	kSpeechAnd = 6,		// "... and" before Frankie
	kSpeechOhShit = 7		// title: the name of the game
};

// Title and high scores
[[noreturn]] void	Title();
void		DrawHiscores();
void		EnterHiscore();

// Player
void		InitPlayer();
void		SetPlayerSprite();
void		UpdatePlayer();
void		AnimatePlayer();
void		SetPlayerCentre();
void		CheckGhostCollision();

// Ghosts
void		InitGhosts();
void		CopyGhostSprites();
void		HideGhosts();
void		UpdateGhosts();
void		AnimateGhosts();
bool		GhostScorePause();

// Fruit
void		InitFruit();
void		UpdateFruit();

// Level end and intermissions
void		CheckLevelDone();
void		RedrawFruitList();

// Joystick test
void		JoystickTest();

// LD A,R of the original (only the demo's random numbers use it). The
// tests replay the values of the original.
extern std::function<uint8_t()> gRefreshValue;


#endif	// OS_GAME_H
