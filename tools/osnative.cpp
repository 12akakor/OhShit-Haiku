/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// The rewritten game without a window.
//
//   osnative game.(rom|cas|dsk) frames [options]
//     -out dir           output folder (default .)
//     -shot f[,f...]     screenshot (PPM) after these frames
//     -every n           screenshot every n frames
//     -key f:row,bit:n   hold a key from frame f for n frames
//     -joy f:bits:n      hold joystick 1 bits from frame f for n frames
//     -wav file          the sound (stereo 44.1 kHz WAV), as the game makes it

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../src/game/Game.h"
#include "../src/game/GameFile.h"
#include "../src/audio/PsgSound.h"


namespace {

struct Hold {
	long	from;
	long	frames;
	int		row;
	int		bit;
};

struct Stop {};


bool
ReadFile(const std::string& path, std::vector<uint8_t>& data)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (f == NULL)
		return false;
	data.clear();
	int c;
	while ((c = fgetc(f)) != EOF)
		data.push_back(static_cast<uint8_t>(c));
	fclose(f);
	return true;
}


void
WriteShot(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (f == NULL)
		return;
	fprintf(f, "P6\n%d %d\n255\n", Tms9918::kWidth, Tms9918::kLines);
	const uint8_t* frame = gVdp.Frame();
	for (int i = 0; i < Tms9918::kWidth * Tms9918::kLines; i++) {
		uint32_t rgb = Tms9918::Rgb(frame[i]);
		fputc(static_cast<int>(rgb >> 16), f);
		fputc(static_cast<int>((rgb >> 8) & 0xff), f);
		fputc(static_cast<int>(rgb & 0xff), f);
	}
	fclose(f);
}

}	// namespace


int
main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: osnative game frames [options]\n");
		return 1;
	}
	long frames = atol(argv[2]);
	std::string outDir = ".";
	std::vector<long> shots;
	long every = 0;
	std::vector<Hold> holds;
	std::string wavPath;
	for (int i = 3; i + 1 < argc; i += 2) {
		std::string a = argv[i];
		std::string v = argv[i + 1];
		if (a == "-out")
			outDir = v;
		else if (a == "-shot") {
			for (char* p = strtok(&v[0], ","); p != NULL; p = strtok(NULL, ","))
				shots.push_back(atol(p));
		} else if (a == "-every")
			every = atol(v.c_str());
		else if (a == "-wav")
			wavPath = v;
		else if (a == "-key" || a == "-joy") {
			Hold h;
			if (a == "-key")
				sscanf(v.c_str(), "%ld:%d,%d:%ld", &h.from, &h.row, &h.bit, &h.frames);
			else {
				sscanf(v.c_str(), "%ld:%d:%ld", &h.from, &h.row, &h.frames);
				h.bit = -1;
			}
			holds.push_back(h);
		}
	}

	std::vector<uint8_t> file;
	if (!ReadFile(argv[1], file)) {
		fprintf(stderr, "cannot read %s\n", argv[1]);
		return 1;
	}
	static GameImage image;
	std::string error;
	if (!LoadGameFile(file, &image, &error)) {
		fprintf(stderr, "%s\n", error.c_str());
		return 1;
	}
	printf("loaded from a %s\n", image.source.c_str());
	InstallGameImage(image);

	static PsgSound sound;
	static std::vector<int16_t> samples;
	gHost.psgWrite = [&](int reg, uint8_t value, uint64_t cycle) {
		sound.Write(cycle, reg, value);
	};
	long frame = 0;
	gHost.frame = [&]() {
		if (!wavPath.empty())
			sound.RenderTo(gClock.cycles, samples);
		// render the finished frame
		for (int line = 0; line < Tms9918::kLines; line++)
			gVdp.RenderLine(line);
		gVdp.StartVBlank();
		frame++;
		bool shot = every > 0 && frame % every == 0;
		for (long s : shots)
			shot = shot || s == frame;
		if (shot) {
			char name[64];
			snprintf(name, sizeof(name), "/shot_%06ld.ppm", frame);
			WriteShot(outDir + name);
		}
		if (frame >= frames)
			throw Stop();
		memset(gKeyMatrix, 0xff, sizeof(gKeyMatrix));
		gJoystick[0] = 0;
		for (const Hold& h : holds) {
			if (frame < h.from || frame >= h.from + h.frames)
				continue;
			if (h.bit < 0)
				gJoystick[0] |= static_cast<uint8_t>(h.row);
			else
				gKeyMatrix[h.row] &= static_cast<uint8_t>(~(1 << h.bit));
		}
	};
	try {
		Start();
	} catch (const Stop&) {
	}
	printf("%ld frames\n", frame);
	if (!wavPath.empty()) {
		FILE* f = fopen(wavPath.c_str(), "wb");
		if (f != NULL) {
			uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
			uint32_t v;
			uint16_t h;
			fwrite("RIFF", 1, 4, f);
			v = 36 + dataSize;
			fwrite(&v, 4, 1, f);
			fwrite("WAVEfmt ", 1, 8, f);
			v = 16;
			fwrite(&v, 4, 1, f);
			h = 1;
			fwrite(&h, 2, 1, f);
			h = 2;
			fwrite(&h, 2, 1, f);
			v = PsgSound::kRate;
			fwrite(&v, 4, 1, f);
			v = PsgSound::kRate * 4;
			fwrite(&v, 4, 1, f);
			h = 4;
			fwrite(&h, 2, 1, f);
			h = 16;
			fwrite(&h, 2, 1, f);
			fwrite("data", 1, 4, f);
			fwrite(&dataSize, 4, 1, f);
			fwrite(samples.data(), 2, samples.size(), f);
			fclose(f);
		}
	}
	return 0;
}
