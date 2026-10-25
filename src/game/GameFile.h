/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef OS_GAME_FILE_H
#define OS_GAME_FILE_H


// Reading the game from the player's file: a cassette image (.cas), a disk
// image (.dsk) or a ROM image (.rom) of Oh Shit!. All of them hold the same
// program; what is taken from them is the program image #8107-#EFFF as the
// original Aackosoft cassette loads it.

#include <stdint.h>

#include <string>
#include <vector>


struct GameImage {
	uint8_t		bytes[0x10000];	// only #8107-#A391 and #A500-#EFFF are set
	std::string	source;			// what kind of file it came from
	bool		patched = false;	// a ROM conversion with changed code
};

bool		LoadGameFile(const std::vector<uint8_t>& file, GameImage* image,
				std::string* error);

// Puts the image into gMem and sets the start state (system variables,
// page 1 bytes for the random numbers).
void		InstallGameImage(const GameImage& image);


#endif	// OS_GAME_FILE_H
