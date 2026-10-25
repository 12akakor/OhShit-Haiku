/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Headless runner of the original program on the MSX1 test machine.
//
//   osrun game.rom frames [options]
//     -bios dir          C-BIOS folder (default /usr/share/cbios)
//     -out dir           output folder (default .)
//     -shot f[,f...]     screenshot (PPM) after these frames
//     -every n           screenshot every n frames
//     -key f:row,bit:n   hold a key from frame f for n frames
//     -joy f:bits:n      hold joystick bits from frame f for n frames
//     -wav file          write the PSG output (speech included) as a WAV
//     -cov file          write the coverage (see below) at the end; it
//                        starts when the game starts (#8107), after the
//                        loader has copied it
//     -random seed       random joystick play (direction changes, fire)
//     -bios-calls        print the BIOS entry points the game calls
//     -ports             print which code uses which I/O ports, and how often
//     -ram f:file        write the 64 KB RAM after frame f
//     -vram f:file       write the 16 KB VRAM after frame f
//
// Coverage file: 65536 bytes for the RAM of slot 3 plus 32768 for the
// cartridge; per byte bit 0 = start of an executed instruction, bit 1 =
// read, bit 2 = written.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "../src/msx1/Machine1.h"


namespace {

struct Hold {
	long	from;
	long	frames;
	int		row;
	int		bit;	// -1: joystick, 'row' holds the bits
};


bool
ReadFile(const std::string& path, std::vector<uint8_t>& data)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (f == NULL)
		return false;
	data.clear();
	uint8_t buffer[65536];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
		data.insert(data.end(), buffer, buffer + n);
	fclose(f);
	return true;
}


void
WriteFile(const std::string& path, const void* data, size_t size)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (f == NULL) {
		fprintf(stderr, "cannot write %s\n", path.c_str());
		return;
	}
	fwrite(data, 1, size, f);
	fclose(f);
}


void
WriteShot(const std::string& path, const Tms9918& vdp)
{
	std::vector<uint8_t> out;
	char header[32];
	int n = snprintf(header, sizeof(header), "P6\n%d %d\n255\n",
		Tms9918::kWidth, Tms9918::kLines);
	out.insert(out.end(), header, header + n);
	const uint8_t* frame = vdp.Frame();
	for (int i = 0; i < Tms9918::kWidth * Tms9918::kLines; i++) {
		uint32_t rgb = Tms9918::Rgb(frame[i]);
		out.push_back(static_cast<uint8_t>(rgb >> 16));
		out.push_back(static_cast<uint8_t>(rgb >> 8));
		out.push_back(static_cast<uint8_t>(rgb));
	}
	WriteFile(path, out.data(), out.size());
}


void
WriteWav(const std::string& path, const std::vector<Machine1::PsgWrite>& log,
	uint64_t endTime)
{
	const int kRate = 44100;
	const double kCpu = 3579545.0;
	Psg psg;
	size_t count = static_cast<size_t>(endTime / kCpu * kRate);
	std::vector<int16_t> samples(count);
	size_t next = 0;
	for (size_t i = 0; i < count; i++) {
		double t = i * kCpu / kRate;
		while (next < log.size() && log[next].time <= t) {
			psg.Write(log[next].reg, log[next].value);
			next++;
		}
		psg.Render(&samples[i], 1, kRate);
	}
	uint32_t dataSize = static_cast<uint32_t>(count * 2);
	uint8_t h[44];
	memcpy(h, "RIFF", 4);
	uint32_t v = 36 + dataSize;
	memcpy(h + 4, &v, 4);
	memcpy(h + 8, "WAVEfmt ", 8);
	v = 16;
	memcpy(h + 16, &v, 4);
	uint16_t s = 1;
	memcpy(h + 20, &s, 2);	// PCM
	memcpy(h + 22, &s, 2);	// mono
	v = kRate;
	memcpy(h + 24, &v, 4);
	v = kRate * 2;
	memcpy(h + 28, &v, 4);
	s = 2;
	memcpy(h + 32, &s, 2);
	s = 16;
	memcpy(h + 34, &s, 2);
	memcpy(h + 36, "data", 4);
	memcpy(h + 40, &dataSize, 4);
	FILE* f = fopen(path.c_str(), "wb");
	if (f == NULL)
		return;
	fwrite(h, 1, 44, f);
	fwrite(samples.data(), 2, count, f);
	fclose(f);
}


// Bus wrapper that records the coverage.
uint8_t* gCoverage = NULL;
Machine1* gMachine = NULL;


uint8_t*
CoverageCell(uint16_t address)
{
	int slot = (gMachine->SlotRegister() >> ((address >> 14) * 2)) & 3;
	if (slot == 3)
		return gCoverage + address;
	if (slot == 1 && address >= 0x4000 && address < 0xc000)
		return gCoverage + 0x10000 + (address - 0x4000);
	return NULL;
}


class CoverageMachine : public Machine1 {
public:
	virtual	uint8_t			Read(uint16_t address)
							{
								if (gCoverage != NULL) {
									uint8_t* c = CoverageCell(address);
									if (c != NULL)
										*c |= 2;
								}
								return Machine1::Read(address);
							}
	virtual	void			Write(uint16_t address, uint8_t value)
							{
								if (gCoverage != NULL) {
									uint8_t* c = CoverageCell(address);
									if (c != NULL)
										*c |= 4;
								}
								Machine1::Write(address, value);
							}
};

}	// namespace


int
main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: osrun game.rom frames [options]\n");
		return 1;
	}
	std::string romPath = argv[1];
	long frames = atol(argv[2]);
	std::string biosDir = "/usr/share/cbios";
	std::string outDir = ".";
	std::vector<long> shots;
	long every = 0;
	std::vector<Hold> holds;
	std::string wavPath, covPath;
	std::vector<std::pair<long, std::string> > ramDumps, vramDumps;
	bool ports = false;
	bool biosCalls = false;
	long randomSeed = -1;

	for (int i = 3; i < argc; i++) {
		std::string a = argv[i];
		std::string v = i + 1 < argc ? argv[i + 1] : "";
		if (a == "-bios") {
			biosDir = v;
			i++;
		} else if (a == "-out") {
			outDir = v;
			i++;
		} else if (a == "-shot") {
			for (char* p = strtok(&v[0], ","); p != NULL; p = strtok(NULL, ","))
				shots.push_back(atol(p));
			i++;
		} else if (a == "-every") {
			every = atol(v.c_str());
			i++;
		} else if (a == "-key" || a == "-joy") {
			Hold h;
			if (a == "-key") {
				if (sscanf(v.c_str(), "%ld:%d,%d:%ld", &h.from, &h.row, &h.bit,
						&h.frames) != 4)
					return 1;
			} else {
				if (sscanf(v.c_str(), "%ld:%d:%ld", &h.from, &h.row, &h.frames) != 3)
					return 1;
				h.bit = -1;
			}
			holds.push_back(h);
			i++;
		} else if (a == "-wav") {
			wavPath = v;
			i++;
		} else if (a == "-cov") {
			covPath = v;
			i++;
		} else if (a == "-random") {
			randomSeed = atol(v.c_str());
			i++;
		} else if (a == "-bios-calls") {
			biosCalls = true;
		} else if (a == "-ports") {
			ports = true;
		} else if (a == "-ram" || a == "-vram") {
			size_t colon = v.find(':');
			std::pair<long, std::string> d(atol(v.substr(0, colon).c_str()),
				v.substr(colon + 1));
			(a == "-ram" ? ramDumps : vramDumps).push_back(d);
			i++;
		} else {
			fprintf(stderr, "unknown option %s\n", a.c_str());
			return 1;
		}
	}

	std::vector<uint8_t> bios, logo, rom;
	if (!ReadFile(biosDir + "/cbios_main_msx1.rom", bios)
		|| !ReadFile(romPath, rom)) {
		fprintf(stderr, "cannot read the BIOS or the ROM\n");
		return 1;
	}
	ReadFile(biosDir + "/cbios_logo_msx1.rom", logo);

	static CoverageMachine machine;
	gMachine = &machine;
	std::vector<uint8_t> coverage;
	if (!covPath.empty()) {
		coverage.assign(0x18000, 0);
		gCoverage = coverage.data();
	}
	if (!machine.LoadBios(bios, logo) || !machine.LoadCartridge(rom)) {
		fprintf(stderr, "bad BIOS or ROM size\n");
		return 1;
	}
	machine.Reset();
	machine.logPsg = !wavPath.empty();
	// (direction, port, pc) -> count; the PC is that of the next
	// instruction, which is close enough to find the routine.
	std::map<uint32_t, uint64_t> portUse;
	std::map<uint32_t, uint64_t> biosUse;
	uint16_t lastPc = 0;
	bool coverageStarted = false;
	if (biosCalls || !covPath.empty()) {
		machine.onStep = [&]() {
			uint16_t pc = machine.Cpu().r.pc;
			if (pc == 0x8107 && !coverageStarted && !coverage.empty()) {
				coverageStarted = true;
				memset(coverage.data(), 0, coverage.size());
			}
			if (gCoverage != NULL) {
				uint8_t* c = CoverageCell(pc);
				if (c != NULL)
					*c |= 1;
			}
			if (biosCalls && pc < 0x4000 && lastPc >= 0x8000 && lastPc != 0xffff) {
				// called from the game: the return address is on the stack
				uint16_t sp = machine.Cpu().r.sp;
				uint16_t ret = static_cast<uint16_t>(machine.Read(sp)
					| (machine.Read(static_cast<uint16_t>(sp + 1)) << 8));
				biosUse[(static_cast<uint32_t>(pc) << 16) | ret]++;
			}
			lastPc = pc;
		};
	}
	if (ports) {
		machine.onPortOut = [&](uint16_t port, uint8_t) {
			portUse[0x1000000u | (port << 16) | machine.Cpu().r.pc]++;
		};
		machine.onPortIn = [&](uint16_t port, uint8_t) {
			portUse[(port << 16) | machine.Cpu().r.pc]++;
		};
	}

	uint32_t rnd = static_cast<uint32_t>(randomSeed) * 2654435761u + 1;
	uint8_t randomJoy = 0;
	for (long f = 0; f < frames; f++) {
		uint8_t joy = 0;
		if (randomSeed >= 0) {
			rnd = rnd * 1103515245u + 12345u;
			uint32_t x = rnd >> 16;
			if (x % 20 == 0) {
				static const uint8_t kDirs[] = { 1, 2, 4, 8, 0, 5, 9 };
				randomJoy = kDirs[(x / 20) % 7];
			}
			joy = randomJoy;
			if ((x >> 8) % 40 == 0)
				joy |= 0x10;
		}
		for (int r = 0; r < 11; r++)
			for (int b = 0; b < 8; b++)
				machine.SetKey(r, b, false);
		for (const Hold& h : holds) {
			if (f < h.from || f >= h.from + h.frames)
				continue;
			if (h.bit < 0)
				joy |= static_cast<uint8_t>(h.row);
			else
				machine.SetKey(h.row, h.bit, true);
		}
		machine.SetJoystick(joy);
		machine.RunFrame();
		long done = f + 1;
		bool shot = every > 0 && done % every == 0;
		for (long s : shots)
			shot = shot || s == done;
		if (shot) {
			char name[64];
			snprintf(name, sizeof(name), "/shot_%06ld.ppm", done);
			WriteShot(outDir + name, machine.Vdp());
		}
		for (auto& d : ramDumps)
			if (d.first == done)
				WriteFile(d.second, machine.Ram(), 0x10000);
		for (auto& d : vramDumps)
			if (d.first == done)
				WriteFile(d.second, machine.Vdp().Vram(), 0x4000);
	}
	if (!wavPath.empty())
		WriteWav(wavPath, machine.psgLog, machine.Time());
	if (!covPath.empty())
		WriteFile(covPath, coverage.data(), coverage.size());
	for (auto& u : biosUse) {
		printf("bios %04X from %04X  %llu\n", u.first >> 16, u.first & 0xffff,
			static_cast<unsigned long long>(u.second));
	}
	for (auto& u : portUse) {
		printf("%s %02X pc %04X  %llu\n", (u.first >> 24) ? "out" : "in ",
			(u.first >> 16) & 0xff, u.first & 0xffff,
			static_cast<unsigned long long>(u.second));
	}
	return 0;
}
