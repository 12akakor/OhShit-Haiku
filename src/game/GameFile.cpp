/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Reading the game from a cassette, disk or ROM image, see GameFile.h.
//
// Known files (see docs/01-progress.md):
// - Aackosoft cassette: BASIC loader, then binary blocks; the game is the
//   blocks loaded at #8107 and #A500.
// - Disk: OHSHIT.001 (#8800-#C851: #8107-#C0B5 moved up by #6F9) and
//   OHSHIT.002 (#87B0: an 80 byte stub, then #C106-#EFFF).
// - ROM conversion "7372": RAM #8000-#BFFF at ROM #0000, #C000-#EFFF at
//   ROM #4800.
// - ROM conversions "1698" and "5640": RAM #8000-#BFFF at ROM #4000,
//   #C000-#C13F at ROM #00C0, #C140-#EF3F at ROM #0200. They were dumped
//   from a running game, so variables and high scores differ.
// - ByteBusters/Eaglesoft cassettes (also on MSX Compilation 5): their own
//   loader and blocks; the data block is #A500-#EFFF, the program block a
//   rebuilt version of the program with the tables at other places. Only
//   the tables are taken from it (the rewrite has its own code).
// The program is checked with checksums: the code and data must be those
// of the original; the variables (their start values are set by the game
// itself) may differ.
#include "GameFile.h"

#include <string.h>

#include "Bios.h"
#include "Machine.h"


namespace {

const uint32_t kCodeCrc = 0xec86e08e;	// #8107-#A391, variables cleared
const uint32_t kDataCrc = 0xf69c942a;	// #A500-#EF3F
const uint32_t kTablesCrc = 0x3fcd69fb;	// the tables below, variables cleared

struct Range {
	uint16_t	start;
	uint16_t	end;
};

// Variables inside the program image, and the copyright line of the title
// (blank in one release).
const Range kVariables[] = {
	{ 0x8115, 0x8198 }, { 0x85ca, 0x85d8 }, { 0x87de, 0x87e3 },
	{ 0x88a7, 0x88ad }, { 0x8be0, 0x8c26 }, { 0x8c86, 0x8ca6 },
	{ 0x8d41, 0x8d7f },
	{ 0x8da3, 0x8da5 }, { 0x8dd6, 0x8ddc }, { 0x9602, 0x9636 },
	{ 0x9914, 0x9921 }, { 0x995b, 0x995d }, { 0x9965, 0x9967 },
	{ 0x9aff, 0x9b0e }, { 0x9b26, 0x9b40 }, { 0x9b6d, 0x9b6e },
	{ 0xa042, 0xa044 }, { 0xa04e, 0xa054 }
};


uint32_t
Crc32(const uint8_t* data, size_t size)
{
	uint32_t crc = 0xffffffff;
	for (size_t i = 0; i < size; i++) {
		crc ^= data[i];
		for (int k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xedb88320 & (0u - (crc & 1)));
	}
	return ~crc;
}


bool
CheckImage(const GameImage& image)
{
	uint8_t code[0xa392 - 0x8107];
	memcpy(code, image.bytes + 0x8107, sizeof(code));
	for (const Range& r : kVariables)
		memset(code + (r.start - 0x8107), 0, r.end - r.start);
	if (Crc32(code, sizeof(code)) != kCodeCrc)
		return false;
	return Crc32(image.bytes + 0xa500, 0xef40 - 0xa500) == kDataCrc;
}


void
Copy(GameImage* image, uint16_t address, const std::vector<uint8_t>& file,
	size_t offset, size_t size)
{
	if (offset + size > file.size() || address + size > 0x10000)
		return;
	memcpy(image->bytes + address, file.data() + offset, size);
}


// --- cassette ---

const uint8_t kCasHeader[8] = { 0x1f, 0xa6, 0xde, 0xba, 0xcc, 0x13, 0x7d, 0x74 };


std::vector<std::vector<uint8_t> >
CasBlocks(const std::vector<uint8_t>& file)
{
	std::vector<size_t> starts;
	for (size_t i = 0; i + 8 <= file.size(); i++) {
		if (memcmp(file.data() + i, kCasHeader, 8) == 0)
			starts.push_back(i + 8);
	}
	std::vector<std::vector<uint8_t> > blocks;
	for (size_t k = 0; k < starts.size(); k++) {
		size_t end = k + 1 < starts.size() ? starts[k + 1] - 8 : file.size();
		blocks.push_back(std::vector<uint8_t>(file.begin() + starts[k],
			file.begin() + end));
	}
	return blocks;
}


bool
LoadCas(const std::vector<uint8_t>& file, GameImage* image)
{
	std::vector<std::vector<uint8_t> > blocks = CasBlocks(file);
	bool any = false;
	for (size_t k = 0; k + 1 < blocks.size(); k++) {
		const std::vector<uint8_t>& header = blocks[k];
		if (header.size() < 16)
			continue;
		bool binary = true;
		for (int i = 0; i < 10; i++)
			binary = binary && header[i] == 0xd0;
		if (!binary)
			continue;
		const std::vector<uint8_t>& data = blocks[k + 1];
		if (data.size() < 6)
			continue;
		uint16_t start = static_cast<uint16_t>(data[0] | (data[1] << 8));
		uint16_t end = static_cast<uint16_t>(data[2] | (data[3] << 8));
		if (start != 0x8107 && start != 0xa500)
			continue;
		if (end < start || data.size() < 6u + (end - start + 1))
			continue;
		memcpy(image->bytes + start, data.data() + 6, end - start + 1);
		any = true;
	}
	return any;
}


// ByteBusters: where the tables of #8107-#A391 are in its program block.
struct TablePlace {
	uint16_t	address;
	uint16_t	size;
	uint16_t	offset;
};

const TablePlace kByteBustersTables[] = {
	{ 0x8115, 131, 40 }, { 0x84ff, 31, 1063 }, { 0x85aa, 32, 1260 },
	{ 0x87be, 37, 3220 }, { 0x88a7, 32, 3453 }, { 0x8bc0, 540, 4280 },
	{ 0x9602, 75, 6906 }, { 0x9914, 83, 7692 }, { 0x9aff, 111, 8183 },
	{ 0xa04e, 6, 9571 }, { 0xa1c8, 458, 9949 }
};


bool
IsVariable(uint16_t address)
{
	for (const Range& r : kVariables) {
		if (address >= r.start && address < r.end)
			return true;
	}
	return false;
}


bool
LoadByteBusters(const std::vector<uint8_t>& file, GameImage* image)
{
	std::vector<std::vector<uint8_t> > blocks = CasBlocks(file);
	const std::vector<uint8_t>* data = NULL;
	const std::vector<uint8_t>* program = NULL;
	for (size_t k = 0; k + 1 < blocks.size(); k++) {
		const std::vector<uint8_t>& header = blocks[k];
		if (header.size() < 14 || header[0] != 0xab || header[1] != 0xcd
			|| header[2] != 0xef)
			continue;
		if (memcmp(header.data() + 3, "OHSHIT  DAT", 11) == 0)
			data = &blocks[k + 1];
		else if (memcmp(header.data() + 3, "OHSHIT  PRG", 11) == 0)
			program = &blocks[k + 1];
	}
	if (data == NULL || program == NULL || data->size() < 0xf000 - 0xa500)
		return false;
	memcpy(image->bytes + 0xa500, data->data(), 0xf000 - 0xa500);
	std::vector<uint8_t> tables;
	for (const TablePlace& t : kByteBustersTables) {
		if (t.offset + t.size > program->size())
			return false;
		memcpy(image->bytes + t.address, program->data() + t.offset, t.size);
		for (int i = 0; i < t.size; i++) {
			uint16_t a = static_cast<uint16_t>(t.address + i);
			tables.push_back(IsVariable(a) ? 0 : image->bytes[a]);
		}
	}
	if (Crc32(tables.data(), tables.size()) != kTablesCrc)
		return false;
	// The jump the interrupt hook copies (code, at the original's address).
	image->bytes[0x8538] = 0xc3;
	image->bytes[0x8539] = 0x3b;
	image->bytes[0x853a] = 0x85;
	return Crc32(image->bytes + 0xa500, 0xef40 - 0xa500) == kDataCrc;
}


// --- disk (FAT12, 720 KB or smaller) ---

bool
DiskFile(const std::vector<uint8_t>& disk, const char* name,
	std::vector<uint8_t>* out)
{
	if (disk.size() < 0x200)
		return false;
	const uint8_t* b = disk.data();
	int bps = b[11] | (b[12] << 8);
	int spc = b[13];
	int reserved = b[14] | (b[15] << 8);
	int fats = b[16];
	int entries = b[17] | (b[18] << 8);
	int spf = b[22] | (b[23] << 8);
	if (bps != 512 || spc == 0 || fats == 0)
		return false;
	size_t fat = static_cast<size_t>(reserved) * bps;
	size_t root = fat + static_cast<size_t>(fats) * spf * bps;
	size_t data = root + static_cast<size_t>(entries) * 32;
	for (int i = 0; i < entries; i++) {
		size_t e = root + i * 32;
		if (e + 32 > disk.size())
			return false;
		if (memcmp(b + e, name, 11) != 0)
			continue;
		int cluster = b[e + 26] | (b[e + 27] << 8);
		uint32_t size = b[e + 28] | (b[e + 29] << 8) | (b[e + 30] << 16)
			| (static_cast<uint32_t>(b[e + 31]) << 24);
		out->clear();
		int guard = 0;
		while (cluster >= 2 && cluster < 0xff8 && ++guard < 4096) {
			size_t offset = data + static_cast<size_t>(cluster - 2) * spc * bps;
			size_t n = static_cast<size_t>(spc) * bps;
			if (offset + n > disk.size())
				return false;
			out->insert(out->end(), b + offset, b + offset + n);
			size_t f = fat + cluster * 3 / 2;
			int v = b[f] | (b[f + 1] << 8);
			cluster = (cluster & 1) ? v >> 4 : v & 0xfff;
		}
		if (out->size() < size)
			return false;
		out->resize(size);
		return true;
	}
	return false;
}


bool
LoadDsk(const std::vector<uint8_t>& disk, GameImage* image)
{
	std::vector<uint8_t> part1, part2;
	if (!DiskFile(disk, "OHSHIT  001", &part1)
		|| !DiskFile(disk, "OHSHIT  002", &part2))
		return false;
	// BLOAD files: #FE, start, end, exec
	if (part1.size() < 7 + (0xc0b6 - 0x8107) || part2.size() < 7 + 80)
		return false;
	// Part 1 holds #8107-#C105 (and the gap #A392-#A4FF, not used),
	// part 2 after its 80 byte stub #C106 on.
	if (part1.size() < 7 + (0xc106 - 0x8107))
		return false;
	Copy(image, 0x8107, part1, 7, 0xa392 - 0x8107);
	Copy(image, 0xa500, part1, 7 + (0xa500 - 0x8107), 0xc106 - 0xa500);
	size_t rest = part2.size() - 7 - 80;
	if (rest > 0xf000 - 0xc106)
		rest = 0xf000 - 0xc106;
	Copy(image, 0xc106, part2, 7 + 80, rest);
	return true;
}


// --- ROM ---

bool
LoadRom(const std::vector<uint8_t>& rom, GameImage* image, int layout)
{
	if (rom.size() != 0x8000 || rom[0] != 'A' || rom[1] != 'B')
		return false;
	if (layout == 0) {
		Copy(image, 0x8000, rom, 0x0000, 0x4000);
		Copy(image, 0xc000, rom, 0x4800, 0x3000);
	} else {
		Copy(image, 0x8000, rom, 0x4000, 0x4000);
		Copy(image, 0xc000, rom, 0x00c0, 0x0140);
		Copy(image, 0xc140, rom, 0x0200, 0x2e00);
	}
	return true;
}

}	// namespace


bool
LoadGameFile(const std::vector<uint8_t>& file, GameImage* image,
	std::string* error)
{
	struct Try {
		const char* kind;
		int which;
	};
	static const Try kTries[] = {
		{ "cassette image", 0 }, { "disk image", 1 }, { "ROM image", 2 },
		{ "ROM image", 3 }, { "cassette image (ByteBusters)", 4 }
	};
	for (const Try& t : kTries) {
		memset(image->bytes, 0, sizeof(image->bytes));
		bool ok;
		switch (t.which) {
			case 0:
				ok = LoadCas(file, image);
				break;
			case 1:
				ok = LoadDsk(file, image);
				break;
			case 2:
				ok = LoadRom(file, image, 0);
				break;
			case 3:
				ok = LoadRom(file, image, 1);
				break;
			default:
				ok = LoadByteBusters(file, image);
				break;
		}
		if (!ok)
			continue;
		if (t.which != 4 && !CheckImage(*image))
			continue;
		image->source = t.kind;
		return true;
	}
	*error = "This is not a known version of Oh Shit! (the Aackosoft and"
		" ByteBusters cassettes, the disk and the ROM versions are supported).";
	return false;
}


void
InstallGameImage(const GameImage& image)
{
	memset(gMem, 0, sizeof(gMem));
	memcpy(gMem + 0x8107, image.bytes + 0x8107, 0xa392 - 0x8107);
	memcpy(gMem + 0xa500, image.bytes + 0xa500, 0xf000 - 0xa500);
	bios::InitSystemVariables();
	// Page 1 as in the ROM version: the program image from #9000 (the
	// ByteBusters file has no code there: then fixed pseudo random bytes).
	memcpy(gPage1, gMem + 0x9000, sizeof(gPage1));
	bool empty = true;
	for (uint8_t b : gPage1)
		empty = empty && b == 0;
	if (empty) {
		uint32_t x = 12345;
		for (uint8_t& b : gPage1) {
			x = x * 1103515245u + 12345u;
			b = static_cast<uint8_t>(x >> 16);
		}
	}
	gVdp.Reset();
}
