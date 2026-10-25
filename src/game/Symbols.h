/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_SYMBOLS_H
#define OS_SYMBOLS_H


// Addresses of the original program, generated from tools/dis/names.txt
// by tools/dis/gen_symbols.py. The port keeps the variables at these
// addresses, so every routine can be compared with the original.

#include <stdint.h>


namespace sym {

const uint16_t SavedSP = 0x8115;	// stack pointer at start, restored on every restart
const uint16_t Lives = 0x8117;
const uint16_t Level = 0x8118;	// 0 = first maze
const uint16_t DemoMode = 0x8119;	// 1 while the attract demo plays
const uint16_t Score = 0x811a;	// 6 decimal digits, most significant first; +4 = tens (a dot), +3 = hundreds
const uint16_t ScoreHundreds = 0x811d;
const uint16_t ScoreTens = 0x811e;
const uint16_t ScoreUnits = 0x811f;
const uint16_t PauseSavedText = 0x8120;	// 6 characters under "PAUSED"
const uint16_t PauseSavedPsg = 0x8126;	// PSG registers 0-12 during the pause
const uint16_t TxtPaused = 0x8135;
const uint16_t TxtReady = 0x813b;
const uint16_t Paused = 0x8141;	// the interrupt skips the game updates when set
const uint16_t ExtraLifeGiven = 0x8142;
const uint16_t LoopDelay = 0x8143;	// busy wait per main loop pass (game speed), lower each level
const uint16_t RandomValue = 0x8145;
const uint16_t FruitListPos = 0x8146;	// name table address of the next fruit in the level list
const uint16_t StickToDir = 0x8148;	// GTSTCK 0-8 to direction bits (1 up, 2 right, 4 down, 8 left)
const uint16_t FruitCharByLevel = 0x8151;
const uint16_t SpriteRotation = 0x815f;	// 0-5, rotates the sprite order against the 4 per line limit
const uint16_t SpritePlayer = 0x8160;	// 7 sprites of 4 bytes: player, 4 ghosts, ghost score, fruit
const uint16_t SpriteGhosts = 0x8164;
const uint16_t SpriteGhostScore = 0x8174;
const uint16_t SpriteFruit = 0x8178;
const uint16_t SpriteOut = 0x817c;	// rotated copy sent to the VDP
const uint16_t ColourTableGame = 0x85aa;
const uint16_t SirenPitch = 0x85ca;
const uint16_t SpeechTable = 0x87be;	// 8 samples: start, end (offsets, +#3D00)
const uint16_t SpeechLevel = 0x87de;
const uint16_t HiscorePlace = 0x88a7;
const uint16_t HiscoreOffset = 0x88a8;
const uint16_t HiscoreNamePos = 0x88ab;
const uint16_t HiscoreNamePositions = 0x88ad;
const uint16_t TitleSpriteJoey = 0x88b7;
const uint16_t TitleSpritePaul = 0x88bb;
const uint16_t TitleSpriteWilly = 0x88bf;
const uint16_t TitleSpriteFrankie = 0x88c3;
const uint16_t ColourTableTitle = 0x8bc0;
const uint16_t HiscoreTable = 0x8be0;	// 5 lines of 14 characters
const uint16_t TxtPressFire = 0x8c26;
const uint16_t TxtWrittenBy = 0x8c46;
const uint16_t TxtSpeechBy = 0x8c66;
const uint16_t TxtCopyright = 0x8c86;
const uint16_t TxtSelect = 0x8ca6;
const uint16_t TxtOhShit = 0x8cc6;
const uint16_t TxtGhostNames = 0x8d06;
const uint16_t Alphabet = 0x8d26;
const uint16_t Ghosts = 0x8d45;	// 4 x (y, x, pattern, colour)
const uint16_t GhostX = 0x8d46;
const uint16_t GhostPattern = 0x8d47;
const uint16_t GhostColour = 0x8d48;
const uint16_t GhostHouseDir = 0x8d55;
const uint16_t GhostDir = 0x8d59;
const uint16_t GhostInHouse = 0x8d5d;
const uint16_t GhostHouseTimer = 0x8d62;	// 4 words
const uint16_t GhostLeaving = 0x8d6a;
const uint16_t GhostMoveTimer = 0x8d6e;
const uint16_t GhostEyesTimer = 0x8d72;
const uint16_t FrightTimer = 0x8d76;
const uint16_t GhostState = 0x8d7a;	// 0 normal, 1 eyes going home, #FF blue
const uint16_t GhostStart = 0x8d7f;
const uint16_t FruitTimer = 0x9914;
const uint16_t PlayerStepTimer = 0x9aff;
const uint16_t PlayerMoveTimer = 0x9b00;
const uint16_t PlayerDir = 0x9b01;
const uint16_t Player = 0x9b02;	// y, x, pattern, colour
const uint16_t PlayerCentre = 0x9b06;
const uint16_t PlayerAnimPtr = 0x9b08;
const uint16_t PlayerAnimStep = 0x9b0a;
const uint16_t DotsEaten = 0x9b26;
const uint16_t Frightened = 0x9b27;
const uint16_t Dying = 0x9b28;
const uint16_t GhostsEaten = 0x9b32;
const uint16_t TxtGameOver = 0x9b54;
const uint16_t ReverseGhosts = 0x9b6d;
const uint16_t InInterrupt = 0xa04e;
const uint16_t SpeechData = 0xa500;
const uint16_t MazeChars = 0xdd00;
const uint16_t DigitChars = 0xde80;
const uint16_t LetterChars = 0xdf08;
const uint16_t MazeMap = 0xe500;	// name table, 32 x 24
const uint16_t SpritePatterns = 0xe800;
const uint16_t LivesChars = 0xe820;
const uint16_t SirenRising = 0x85cb;
const uint16_t SirenTone = 0x85cc;
const uint16_t SirenTimer = 0x85cd;
const uint16_t ToneTimer = 0x85ce;
const uint16_t FrightSound = 0x85cf;
const uint16_t EyesSound = 0x85d0;
const uint16_t ChompPitch = 0x85d1;
const uint16_t ChompOn = 0x85d2;
const uint16_t ChompHigh = 0x85d3;
const uint16_t EatGhostPitch = 0x85d5;
const uint16_t EatGhostTimer = 0x85d6;
const uint16_t EatGhostSound = 0x85d7;
const uint16_t SpeechSavedPsg = 0x87df;
const uint16_t PlayerX = 0x9b03;
const uint16_t PlayerPattern = 0x9b04;
const uint16_t PlayerCentreX = 0x9b07;
const uint16_t PlayerAnimTimer = 0x9b0b;
const uint16_t PlayerUnused1 = 0x9b0c;
const uint16_t PlayerUnused2 = 0x9b0d;
const uint16_t AnimLeft = 0x9b0e;
const uint16_t AnimRight = 0x9b13;
const uint16_t AnimUp = 0x9b18;
const uint16_t AnimDown = 0x9b1d;
const uint16_t PlayerStart = 0x9b22;
const uint16_t DeathAnim = 0x9b29;
const uint16_t GhostPoints = 0x9b33;	// hundreds per ghost: 2, 4, 8, 16
const uint16_t GhostScorePatterns = 0x9b37;
const uint16_t GhostScoreSprite = 0x9b3b;
const uint16_t GhostScorePattern = 0x9b3d;
const uint16_t SavedPlayerY = 0x9b3f;
const uint16_t DemoDirs = 0x9b40;
const uint16_t PlayerSpeedByLevel = 0x9b44;
const uint16_t FrightTimeByLevel = 0x9b5d;
const uint16_t ScorePause = 0x8dd9;	// set while the score of an eaten ghost shows
const uint16_t ScorePauseTimer = 0x8dda;
const uint16_t GhostHouseTick = 0x8d41;	// 4 counters, a house step when they wrap
const uint16_t GhostAnimTimer = 0x8d61;
const uint16_t FrightTick = 0x8d7e;
const uint16_t GhostStartHouseDir = 0x8d8f;
const uint16_t GhostStartDir = 0x8d93;
const uint16_t GhostStartInHouse = 0x8d97;
const uint16_t GhostStartHouseTimer = 0x8d9b;
const uint16_t GhostCentrePos = 0x8da3;	// y + 9, x + 8 of the ghost being moved
const uint16_t FrightSlowByLevel = 0x8da5;
const uint16_t GhostSpeedByLevel = 0x8db4;
const uint16_t GhostPatternByDir = 0x8dc4;
const uint16_t EyesPatternByDir = 0x8dcd;
const uint16_t ChaseTarget = 0x8dd6;	// y, x the ghost heads for
const uint16_t ChaseTargetX = 0x8dd7;
const uint16_t FruitCount = 0x9916;
const uint16_t FruitShown = 0x9917;
const uint16_t FruitScoreShown = 0x9918;
const uint16_t FruitSpriteStart = 0x9919;
const uint16_t FruitSprite = 0x991d;
const uint16_t FruitColour = 0x9920;
const uint16_t FruitColourByLevel = 0x9921;
const uint16_t FruitPointsByLevel = 0x9931;	// hundreds
const uint16_t FruitScoreDigits = 0x993f;	// two characters per level
const uint16_t BlinkTimer = 0x995b;
const uint16_t BlinkPhase = 0x995c;
const uint16_t PillChar = 0x995d;
const uint16_t ScatterTimer = 0x9965;
const uint16_t CutSprites = 0x9602;	// 5 sprites (y, x, pattern, colour)
const uint16_t CutSpritesX = 0x9603;
const uint16_t CutSprite0Pattern = 0x9604;
const uint16_t CutSprite1X = 0x9607;
const uint16_t CutSprite1Pattern = 0x9608;
const uint16_t CutSprite2Pattern = 0x960c;
const uint16_t CutSprite4Pattern = 0x9614;
const uint16_t CutStart1 = 0x9616;
const uint16_t CutStart2 = 0x961e;
const uint16_t CutTimerA = 0x9632;
const uint16_t CutTimerB = 0x9633;
const uint16_t CutAnimTimer = 0x9634;
const uint16_t CutAnimStep = 0x9635;
const uint16_t CutPlayerAnim = 0x9636;
const uint16_t CutGhostAnim = 0x963b;
const uint16_t CutShrinkPatterns = 0x9645;
const uint16_t JoyTestPort = 0xa050;	// also the colour of the fire sprite
const uint16_t JoyTestStick = 0xa051;
const uint16_t PrintPointer = 0xa052;
const uint16_t JoyTestDirSprites = 0xa1c8;
const uint16_t JoyTestPatterns = 0xa1d8;
const uint16_t JoyTestSprites = 0xa278;
const uint16_t TxtJoyTitle = 0xa28c;
const uint16_t TxtJoyRelease = 0xa2c5;
const uint16_t TxtJoyPressReturn = 0xa2ef;
const uint16_t TxtNewLine = 0xa306;
const uint16_t TxtJoyPort = 0xa309;
const uint16_t TxtJoyOk = 0xa315;
const uint16_t TxtJoyNotOk = 0xa319;
const uint16_t TxtJoyConnect = 0xa321;
const uint16_t TxtJoyExit = 0xa340;
const uint16_t TxtJoyKeyboard = 0xa35e;


// Routines of the original (for Sync and Restart).
namespace code {

const uint16_t Start = 0x8107;	// entry point (exec address of the cassette block)
const uint16_t Init = 0x8198;	// screen 1 (32x24), colours, sprites
const uint16_t Restart = 0x81b7;	// silence, then the title
const uint16_t NewGame = 0x81bd;
const uint16_t MainLoop = 0x823f;
const uint16_t LoopWait = 0x826e;	// busy wait LoopDelay
const uint16_t CheckEsc = 0x8277;	// ESC: back to the title
const uint16_t PosToName = 0x8287;	// (de) = x,y in pixels -> hl = name table address
const uint16_t Touches = 0x82ac;	// carry if the positions at (hl) and (de) are within 4 pixels
const uint16_t AbsDiffMinus4 = 0x82b6;
const uint16_t AddScore = 0x82c5;	// normalise and print the score, extra life at 10000
const uint16_t NormaliseScore = 0x82f0;
const uint16_t PrintScore = 0x830b;
const uint16_t DrawLives = 0x832a;
const uint16_t DrawNextFruit = 0x8378;
const uint16_t CheckPause = 0x83e1;	// P pauses, P again continues
const uint16_t KeyP = 0x8447;	// Z set if P is pressed
const uint16_t ReadStick = 0x844f;	// keyboard and both joysticks -> direction bits, Z if none
const uint16_t ReadTrigger = 0x8468;	// space and both joysticks
const uint16_t Random = 0x8476;
const uint16_t RandomByte = 0x84a0;	// reads a byte at #5000-#5007 of page 1 (BIOS or cartridge)
const uint16_t DrawMaze = 0x84b2;
const uint16_t ShowReady = 0x84be;
const uint16_t ClearScreen = 0x84f4;
const uint16_t StopInterrupt = 0x851e;	// H.KEYI = RET, sprites off
const uint16_t StartInterrupt = 0x8526;	// H.KEYI = JP Interrupt
const uint16_t Interrupt = 0x853b;	// replaces the BIOS interrupt handler completely
const uint16_t SilencePsg = 0x85d8;
const uint16_t StartSiren = 0x85f1;
const uint16_t UpdateSound = 0x863e;
const uint16_t FrightSoundOn = 0x86bd;
const uint16_t FrightSoundOff = 0x86c3;
const uint16_t EyesSoundOn = 0x86c8;
const uint16_t EyesSoundOff = 0x86d3;
const uint16_t UpdateChomp = 0x86f1;
const uint16_t PlayChomp = 0x8729;
const uint16_t UpdateEatGhostSound = 0x876e;
const uint16_t PlayEatGhostSound = 0x879a;
const uint16_t PsgRead = 0x87b8;
const uint16_t PsgWrite = 0x87bb;
const uint16_t Say = 0x87e3;	// A = sample; saves the PSG, speaks, pauses, restores
const uint16_t PlaySample = 0x882c;	// 1 bit speech on the volume of channel C, interrupts off
const uint16_t PlaySampleLoop = 0x8872;
const uint16_t SpeechPause = 0x889e;
const uint16_t DrawHiscores = 0x88c7;
const uint16_t EnterHiscore = 0x88ef;
const uint16_t InputDelay = 0x8a3b;
const uint16_t Title = 0x8a44;
const uint16_t PrintLine = 0x8b42;	// 32 characters from (hl) to name table de, plus c
const uint16_t LoadCharset = 0x8b4f;
const uint16_t SkipInDemo = 0x8b9d;	// returns from the caller in demo mode
const uint16_t CheckSelect = 0x8ba4;	// SELECT: joystick test
const uint16_t InitGhosts = 0x8ddc;
const uint16_t CopyGhostSprites = 0x8e53;
const uint16_t HideGhosts = 0x8e78;
const uint16_t UpdateGhosts = 0x8e89;
const uint16_t UpdateGhostInHouse = 0x8ebe;
const uint16_t GhostCentre = 0x8f9e;
const uint16_t GhostReverseCheck = 0x8fb9;
const uint16_t GhostChase = 0x8fd6;
const uint16_t GhostSteer = 0x9002;
const uint16_t IsWall = 0x9461;	// carry if the cell at (de) blocks
const uint16_t UpdateFright = 0x9479;
const uint16_t AnimateGhosts = 0x94f2;
const uint16_t UpdateEyes = 0x9536;
const uint16_t GhostScorePause = 0x95be;
const uint16_t GhostReverse = 0x95e5;
const uint16_t CheckLevelDone = 0x964d;	// 205 dots eaten
const uint16_t NextMaze = 0x96b4;
const uint16_t Intermission1 = 0x96cd;
const uint16_t Intermission2 = 0x97dc;
const uint16_t RedrawFruitList = 0x98a4;
const uint16_t CopyIntermissionSprites = 0x98c9;
const uint16_t HideSprites = 0x98fb;
const uint16_t ShortWait = 0x9906;
const uint16_t LongWait = 0x990f;
const uint16_t InitFruit = 0x9967;
const uint16_t UpdateFruit = 0x9988;
const uint16_t BlinkPills = 0x9abc;
const uint16_t UpdateScatter = 0x9ae4;
const uint16_t InitPlayer = 0x9b6e;
const uint16_t SetPlayerSprite = 0x9ba4;
const uint16_t UpdatePlayer = 0x9bcc;
const uint16_t MovePlayer = 0x9c87;
const uint16_t CanMove = 0x9d21;
const uint16_t EatDot = 0x9d3a;
const uint16_t EatPill = 0x9d5b;
const uint16_t SetPlayerCentre = 0x9db7;
const uint16_t AnimatePlayer = 0x9dc8;
const uint16_t CheckGhostCollision = 0x9df2;
const uint16_t PlayerDies = 0x9e12;
const uint16_t GameOver = 0x9e6a;
const uint16_t EatGhost = 0x9e8b;
const uint16_t DemoInput = 0x9f00;
const uint16_t VdpReadAddress = 0x9f3c;
const uint16_t VdpWriteAddress = 0x9f51;
const uint16_t VdpFill = 0x9f65;	// a to VRAM hl, bc bytes
const uint16_t VdpFillEi = 0x9f68;
const uint16_t VdpPoke = 0x9f81;
const uint16_t VdpPeek = 0x9f92;
const uint16_t SetColours = 0x9fa1;	// like BIOS CHGCLR
const uint16_t VdpWriteReg = 0x9fdf;
const uint16_t VdpRead = 0x9ffd;	// VRAM hl to de, bc bytes
const uint16_t VdpWrite = 0xa01b;	// hl to VRAM de, bc bytes
const uint16_t WithDi = 0xa03b;	// runs the rest of the calling routine with interrupts off
const uint16_t WaitKeysUp = 0xa054;
const uint16_t CheckReturn = 0xa067;
const uint16_t PrintText = 0xa088;	// text up to "$" with CHPUT
const uint16_t JoystickTest = 0xa0a7;
const uint16_t JoyTestNotOk = 0xa09b;
const uint16_t JoyTestShowDir = 0xa1a4;
const uint16_t JoyTestLight = 0xa1b9;

}	// namespace code


// BIOS entry points and system variables.
namespace system {

const uint16_t CHKRAM = 0x0000;	// reset
const uint16_t WRTVRM = 0x004d;
const uint16_t LDIRVM = 0x005c;
const uint16_t CHGCLR = 0x0062;
const uint16_t CLRSPR = 0x0069;
const uint16_t INITXT = 0x006c;
const uint16_t INIT32 = 0x006f;
const uint16_t WRTPSG = 0x0093;
const uint16_t RDPSG = 0x0096;
const uint16_t CHPUT = 0x00a2;
const uint16_t CLS = 0x00c3;
const uint16_t GTSTCK = 0x00d5;
const uint16_t GTTRIG = 0x00d8;
const uint16_t SNSMAT = 0x0141;
const uint16_t KILBUF = 0x0156;
const uint16_t T32COL = 0xf3bf;
const uint16_t RG0SAV = 0xf3df;
const uint16_t RG1SAV = 0xf3e0;
const uint16_t FORCLR = 0xf3e9;
const uint16_t BAKCLR = 0xf3ea;
const uint16_t BDRCLR = 0xf3eb;
const uint16_t NEWKEY7 = 0xfbec;	// NEWKEY row 7
const uint16_t JIFFY = 0xfc9e;
const uint16_t SCRMOD = 0xfcaf;
const uint16_t HKEYI = 0xfd9a;	// H.KEYI hook
const uint16_t HTIMI = 0xfd9f;	// H.TIMI hook

}	// namespace system

}	// namespace sym


#endif	// OS_SYMBOLS_H
