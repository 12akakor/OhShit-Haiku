/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_BIOS_H
#define OS_BIOS_H


// The MSX BIOS routines the program calls, rewritten with the effects of
// C-BIOS (the BIOS of the reference machine) on what the game can see:
// video memory and registers, PSG registers, system variables, and whether
// interrupts are enabled afterwards. INIT32 and INITXT load C-BIOS's font,
// as the reference machine does (the game replaces it with its own
// characters; the joystick test shows it).

#include <stdint.h>


namespace bios {

// System variables (MSX standard addresses).
const uint16_t RG0SAV = 0xf3df;
const uint16_t RG1SAV = 0xf3e0;
const uint16_t FORCLR = 0xf3e9;
const uint16_t BAKCLR = 0xf3ea;
const uint16_t BDRCLR = 0xf3eb;
const uint16_t T32NAM = 0xf3bd;
const uint16_t T32COL = 0xf3bf;
const uint16_t T32CGP = 0xf3c1;
const uint16_t T32ATR = 0xf3c3;
const uint16_t T32PAT = 0xf3c5;
const uint16_t TXTNAM = 0xf3b3;
const uint16_t TXTCGP = 0xf3b7;
const uint16_t LINL40 = 0xf3ae;
const uint16_t LINL32 = 0xf3af;
const uint16_t LINLEN = 0xf3b0;
const uint16_t CSRY = 0xf3dc;
const uint16_t CSRX = 0xf3dd;
const uint16_t NEWKEY = 0xfbe5;
const uint16_t JIFFY = 0xfc9e;
const uint16_t SCRMOD = 0xfcaf;
const uint16_t OLDSCR = 0xfcb0;
const uint16_t NAMBAS = 0xf922;
const uint16_t CGPBAS = 0xf924;
const uint16_t PATBAS = 0xf926;
const uint16_t ATRBAS = 0xf928;
const uint16_t HKEYI = 0xfd9a;

// Sets the system variables C-BIOS leaves after start up.
void	InitSystemVariables();

void	WRTVDP(uint8_t reg, uint8_t value);
void	WRTVRM(uint16_t address, uint8_t value);
void	LDIRVM(uint16_t source, uint16_t vram, uint16_t count);
void	CHGCLR();
void	CLRSPR();
void	INITXT();
void	INIT32();
void	CLS();
void	CHPUT(uint8_t c);
void	KILBUF();
void	WRTPSG(uint8_t reg, uint8_t value);
uint8_t	RDPSG(uint8_t reg);
uint8_t	GTSTCK(uint8_t stick);
uint8_t	GTTRIG(uint8_t trigger);
uint8_t	SNSMAT(uint8_t row);

// The BIOS part of the interrupt (H.KEYI returned): status, JIFFY, every
// third time the keyboard.
void	Interrupt();

}	// namespace bios


#endif	// OS_BIOS_H
