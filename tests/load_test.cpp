/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Loads game files and prints what was recognised.
//
//   load_test file...

#include <stdio.h>

#include <string>
#include <vector>

#include "../src/game/GameFile.h"


int
main(int argc, char** argv)
{
	int failures = 0;
	for (int i = 1; i < argc; i++) {
		std::vector<uint8_t> data;
		FILE* f = fopen(argv[i], "rb");
		if (f != NULL) {
			int c;
			while ((c = fgetc(f)) != EOF)
				data.push_back(static_cast<uint8_t>(c));
			fclose(f);
		}
		static GameImage image;
		std::string error;
		if (LoadGameFile(data, &image, &error))
			printf("OK    %-14s %s\n", image.source.c_str(), argv[i]);
		else {
			printf("FAIL  %s: %s\n", argv[i], error.c_str());
			failures++;
		}
	}
	return failures == 0 ? 0 : 1;
}
