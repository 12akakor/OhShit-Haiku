/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Oh Shit! for Haiku and RenkuOS: the game thread. Finds the game file,
// sets up picture and sound, and runs the rewritten program; at the end of
// every video frame of the original (59.92 per second) the picture is
// shown, the sound of that frame is made and the keys are read.
//
// Keys: arrows (or a joystick) move, space fire, P pause, Esc back to the
// title, Page Down is the MSX SELECT key (joystick test on the title),
// Return. Port keys: F6 settings, F7 picture filter, F8 timing display,
// F9 or Print Screen screenshot, F11 full screen, F12 or Alt+Q quit.
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "audio/PsgSound.h"
#include "game/Game.h"
#include "game/GameFile.h"
#include "platform/Platform.h"


namespace {

struct QuitGame {};
struct NewGameFile {};


// MSX keyboard matrix position of a key: row * 8 + bit, -1 for none.
int
MatrixPosition(int key)
{
	if (key >= MOGK_0 && key <= MOGK_7)
		return 0 * 8 + (key - MOGK_0);
	if (key == MOGK_8 || key == MOGK_9)
		return 1 * 8 + (key - MOGK_8);
	if (key >= MOGK_a && key <= MOGK_z) {
		int n = key - MOGK_a + 22;
		return (n / 8) * 8 + n % 8;
	}
	switch (key) {
		case MOGK_MINUS: return 1 * 8 + 2;
		case MOGK_EQUALS: return 1 * 8 + 3;
		case MOGK_BACKSLASH: return 1 * 8 + 4;
		case MOGK_LEFTBRACKET: return 1 * 8 + 5;
		case MOGK_RIGHTBRACKET: return 1 * 8 + 6;
		case MOGK_SEMICOLON: return 1 * 8 + 7;
		case MOGK_QUOTE: return 2 * 8 + 0;
		case MOGK_BACKQUOTE: return 2 * 8 + 1;
		case MOGK_COMMA: return 2 * 8 + 2;
		case MOGK_PERIOD: return 2 * 8 + 3;
		case MOGK_SLASH: return 2 * 8 + 4;
		case MOGK_LSHIFT: case MOGK_RSHIFT: return 6 * 8 + 0;
		case MOGK_LCTRL: case MOGK_RCTRL: return 6 * 8 + 1;
		case MOGK_LALT: return 6 * 8 + 2;	// GRAPH
		case MOGK_CAPSLOCK: return 6 * 8 + 3;
		case MOGK_RALT: return 6 * 8 + 4;	// CODE
		case MOGK_F1: return 6 * 8 + 5;
		case MOGK_F2: return 6 * 8 + 6;
		case MOGK_F3: return 6 * 8 + 7;
		case MOGK_F4: return 7 * 8 + 0;
		case MOGK_F5: return 7 * 8 + 1;
		case MOGK_ESCAPE: return 7 * 8 + 2;
		case MOGK_TAB: return 7 * 8 + 3;
		case MOGK_END: return 7 * 8 + 4;	// STOP
		case MOGK_BACKSPACE: return 7 * 8 + 5;
		case MOGK_PAGEDOWN: return 7 * 8 + 6;	// SELECT
		case MOGK_RETURN: case MOGK_KP_ENTER: return 7 * 8 + 7;
		case MOGK_SPACE: return 8 * 8 + 0;
		case MOGK_HOME: return 8 * 8 + 1;
		case MOGK_INSERT: return 8 * 8 + 2;
		case MOGK_DELETE: return 8 * 8 + 3;
		case MOGK_LEFT: return 8 * 8 + 4;
		case MOGK_UP: return 8 * 8 + 5;
		case MOGK_DOWN: return 8 * 8 + 6;
		case MOGK_RIGHT: return 8 * 8 + 7;
	}
	return -1;
}


// Keys pressed since the last frame count as down for one frame, so a very
// short tap is not lost.
bool sTapped[MOGK_LAST];

void
UpdateMatrix()
{
	PlatPumpEvents();
	unsigned char* state = PlatKeyState();
	for (int r = 0; r < 11; r++)
		gKeyMatrix[r] = 0xff;
	for (int key = 1; key < MOGK_LAST; key++) {
		if (state[key] == 0 && !sTapped[key])
			continue;
		sTapped[key] = false;
		int p = MatrixPosition(key);
		if (p >= 0)
			gKeyMatrix[p / 8] &= static_cast<uint8_t>(~(1 << (p % 8)));
	}
	gJoystick[0] = PlatJoystick();
	gJoystick[1] = 0;
}


bool
ReadFile(const std::string& path, std::vector<uint8_t>& data)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (f == NULL)
		return false;
	data.clear();
	uint8_t buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		data.insert(data.end(), buf, buf + n);
	fclose(f);
	return true;
}


bool
LoadFromPath(const std::string& path, GameImage* image, std::string* error)
{
	std::vector<uint8_t> data;
	if (!ReadFile(path, data)) {
		*error = "cannot read the file";
		return false;
	}
	return LoadGameFile(data, image, error);
}


bool
HasGameExtension(const std::string& name)
{
	if (name.size() < 5)
		return false;
	std::string ext = name.substr(name.size() - 4);
	for (char& c : ext)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	return ext == ".rom" || ext == ".cas" || ext == ".dsk";
}


// The game file: given on the command line, or in the settings folder
// (OhShit.rom, .cas or .dsk first, then any such file that loads).
bool
FindGame(int argc, char** argv, GameImage* image, std::string* message)
{
	std::string error;
	if (argc > 1) {
		if (LoadFromPath(argv[1], image, &error))
			return true;
		*message = std::string(argv[1]) + ": " + error;
		return false;
	}
	std::string dir = PlatSettingsDir();
	for (const char* name : { "OhShit.rom", "OhShit.cas", "OhShit.dsk" }) {
		if (LoadFromPath(dir + "/" + name, image, &error))
			return true;
	}
	DIR* d = opendir(dir.c_str());
	if (d != NULL) {
		for (;;) {
			struct dirent* e = readdir(d);
			if (e == NULL)
				break;
			std::string name = e->d_name;
			if (HasGameExtension(name) && LoadFromPath(dir + "/" + name, image, &error)) {
				closedir(d);
				return true;
			}
		}
		closedir(d);
	}
	*message = "No Oh Shit! game file found.\n\nPut a cassette (.cas), disk (.dsk) or "
		"ROM (.rom) image of Oh Shit! in\n" + dir + "\nor give its path on the command line.";
	return false;
}


// Copies the file the player chose into the settings folder as
// OhShit.<ext>, so it is found next time.
void
KeepGame(const char* path)
{
	std::string p = path;
	std::string ext = p.size() > 4 ? p.substr(p.size() - 4) : ".rom";
	for (char& c : ext)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	if (ext != ".rom" && ext != ".cas" && ext != ".dsk")
		ext = ".rom";
	std::string dir = PlatSettingsDir();
	std::string target = dir + "/OhShit" + ext;
	if (target == p)
		return;
	std::vector<uint8_t> data;
	if (!ReadFile(p, data))
		return;
	for (const char* e : { ".rom", ".cas", ".dsk" }) {
		std::string old = dir + "/OhShit" + e;
		FILE* f = fopen(old.c_str(), "rb");
		if (f != NULL) {
			fclose(f);
			rename(old.c_str(), (old + ".old").c_str());
		}
	}
	FILE* out = fopen(target.c_str(), "wb");
	if (out == NULL)
		return;
	fwrite(data.data(), 1, data.size(), out);
	fclose(out);
}


bool
ChooseGame(std::string message, GameImage* image)
{
	for (;;) {
		std::string text = message + "\n\nChoose the game file now?";
		if (PlatAsk("Oh Shit!", text.c_str(), "Choose file\xE2\x80\xA6", "Quit") != 0)
			return false;
		char path[1024];
		if (!PlatChooseFile("Choose the Oh Shit! game file", path, sizeof(path)))
			continue;
		std::string error;
		if (LoadFromPath(path, image, &error)) {
			KeepGame(path);
			return true;
		}
		message = std::string(path) + ":\n" + error;
	}
}


// Sound: the game thread renders each frame's samples into a ring buffer,
// the audio thread takes them out.
const size_t kRing = 44100 * 2;	// half a second, stereo
std::vector<int16_t> sRing(kRing);
size_t sRead = 0, sWrite = 0, sFill = 0;
size_t sLargestRequest = 0;

void
AudioCallback(int16_t* out, int frames, void*)
{
	size_t need = static_cast<size_t>(frames) * 2;
	PlatAudioLock();
	if (need > sLargestRequest)
		sLargestRequest = need;
	size_t n = need < sFill ? need : sFill;
	for (size_t i = 0; i < n; i++) {
		out[i] = sRing[sRead];
		sRead = (sRead + 1) % kRing;
	}
	sFill -= n;
	PlatAudioUnlock();
	for (size_t i = n; i < need; i++)
		out[i] = 0;
}

size_t
AudioFill(size_t* target)
{
	PlatAudioLock();
	size_t fill = sFill;
	size_t largest = sLargestRequest;
	PlatAudioUnlock();
	*target = 44100 / 20 * 2;
	if (*target < largest * 2)
		*target = largest * 2;
	return fill;
}

void
AudioPush(const std::vector<int16_t>& samples)
{
	PlatAudioLock();
	for (int16_t v : samples) {
		if (sFill == kRing)
			break;
		sRing[sWrite] = v;
		sWrite = (sWrite + 1) % kRing;
		sFill++;
	}
	PlatAudioUnlock();
}


// Settings (settings window, F keys), kept in OhShit.settings.
enum {
	kSetFilter,
	kSetStereo,
	kSetSmoothSound,
	kSetNoFlicker,
	kSetFullscreen,
	kSetVsync,
	kSetCount
};

const char* const kFilterNames[] = { "xbr", "sharp", "nearest" };
const char* const kFilterChoices[] = { "Smooth (xBR)", "Sharp pixels",
	"Pixels, whole steps" };
const char* const kStereoChoices[] = { "Mono", "A little", "Wide" };
const float kStereoWidth[] = { 0.0f, 0.3f, 0.8f };
const char* const kKeys[kSetCount] = { "filter", "stereo", "smoothsound", "noflicker",
	"fullscreen", "vsync" };

PlatSetting sSettings[kSetCount] = {
	{ "Picture:", kFilterChoices, 3, 0, true },
	{ "Stereo:", kStereoChoices, 3, 1, true },
	{ "Softer sound", NULL, 0, 0, true },
	{ "No sprite flicker", NULL, 0, 1, true },
	{ "Full screen", NULL, 0, 0, true },
	{ "Wait for the screen refresh", NULL, 0, 1, true }
};
int sWindowPlace[4] = { 0, 0, 0, 0 };

const char* const kHelp =
	"Keys: arrows (or a joystick) move, space starts, P pauses,\n"
	"Esc goes back to the title, Page Down is SELECT (joystick test).\n"
	"F6 settings, F7 picture, F8 timing, F9 screenshot,\n"
	"F11 full screen, F12 quit.\n"
	"Drop a game file (.cas, .dsk, .rom) on the window to play it.";

std::string
SettingsPath()
{
	return std::string(PlatSettingsDir()) + "/OhShit.settings";
}

void
LoadSettings()
{
	FILE* fp = fopen(SettingsPath().c_str(), "r");
	if (fp == NULL)
		return;
	char line[128], key[32], word[32];
	while (fgets(line, sizeof(line), fp) != NULL) {
		int x, y, w, h;
		if (sscanf(line, "window %d %d %d %d", &x, &y, &w, &h) == 4) {
			sWindowPlace[0] = x;
			sWindowPlace[1] = y;
			sWindowPlace[2] = w;
			sWindowPlace[3] = h;
			continue;
		}
		if (sscanf(line, "%31s %31s", key, word) != 2)
			continue;
		for (int i = 0; i < kSetCount; i++) {
			if (strcmp(key, kKeys[i]) != 0)
				continue;
			int value = atoi(word);
			if (i == kSetFilter) {
				for (int c = 0; c < 3; c++) {
					if (strcmp(word, kFilterNames[c]) == 0)
						value = c;
				}
			}
			if (value >= 0 && value < (sSettings[i].choices != NULL ? sSettings[i].count : 2))
				sSettings[i].value = value;
		}
	}
	fclose(fp);
}

void
SaveSettings()
{
	FILE* fp = fopen(SettingsPath().c_str(), "w");
	if (fp == NULL)
		return;
	fprintf(fp, "# Oh Shit! settings (F6 shows the settings window)\n");
	for (int i = 0; i < kSetCount; i++) {
		if (i == kSetFilter)
			fprintf(fp, "%s %s\n", kKeys[i], kFilterNames[sSettings[i].value]);
		else
			fprintf(fp, "%s %d\n", kKeys[i], sSettings[i].value);
	}
	int x, y, w, h;
	if (PlatGetWindow(&x, &y, &w, &h))
		fprintf(fp, "window %d %d %d %d\n", x, y, w, h);
	fclose(fp);
}


PsgSound sSound;
std::vector<int16_t> sSamples;
bool sAudioOpen = false;
GameImage sNextImage;

void
Apply(int setting)
{
	int value = sSettings[setting].value;
	switch (setting) {
		case kSetFilter:
			PlatSetScaler(kFilterNames[value]);
			break;
		case kSetStereo:
			sSound.SetStereoWidth(kStereoWidth[value]);
			break;
		case kSetSmoothSound:
			sSound.SetSmooth(value != 0);
			break;
		case kSetNoFlicker:
			gVdp.SetSpriteLimit(value == 0);
			break;
		case kSetFullscreen:
			PlatSetFullscreen(value != 0);
			break;
		case kSetVsync:
			PlatSetVsync(value != 0);
			break;
	}
}

void
Change(int setting, int value)
{
	sSettings[setting].value = value;
	Apply(setting);
	PlatUpdateSetting(setting, sSettings[setting].value);
	SaveSettings();
}


// End of a video frame of the original: picture, sound, input, pace.
void
EndOfFrame()
{
	PlatEvent event;
	while (PlatPollEvent(&event)) {
		if (event.type == PLAT_EV_QUIT)
			throw QuitGame();
		if (event.type == PLAT_EV_SETTING) {
			if (event.setting >= 0 && event.setting < kSetCount)
				Change(event.setting, event.value);
		} else if (event.type == PLAT_EV_FILE) {
			char path[1024];
			std::string error;
			if (!PlatDroppedFile(path, sizeof(path)))
				continue;
			if (LoadFromPath(path, &sNextImage, &error)) {
				KeepGame(path);
				throw NewGameFile();
			}
			PlatShowMessage(("Not an Oh Shit! game file: " + error).c_str());
		} else if (event.type == PLAT_EV_KEYDOWN) {
			if (event.key > 0 && event.key < MOGK_LAST)
				sTapped[event.key] = true;
			if (event.key == MOGK_q && (event.modifiers & PLAT_MOD_ALT) != 0)
				throw QuitGame();
			switch (event.key) {
				case MOGK_F6:
					PlatShowSettings(sSettings, kSetCount, kHelp);
					break;
				case MOGK_F7:
					Change(kSetFilter, (sSettings[kSetFilter].value + 1) % 3);
					break;
				case MOGK_F8:
					PlatToggleStats();
					break;
				case MOGK_F9:
				case MOGK_PRINT:
					PlatSaveScreenshot();
					break;
				case MOGK_F11:
					Change(kSetFullscreen, PlatIsFullscreen() ? 0 : 1);
					break;
				case MOGK_F12:
					throw QuitGame();
				default:
					break;
			}
		}
	}
	UpdateMatrix();

	// picture
	for (int line = 0; line < Tms9918::kLines; line++)
		gVdp.RenderLine(line);
	gVdp.StartVBlank();
	uint8_t palette[768];
	memset(palette, 0, sizeof(palette));
	for (int i = 0; i < 16; i++) {
		uint32_t c = Tms9918::Rgb(i);
		palette[i * 3] = static_cast<uint8_t>(c >> 16);
		palette[i * 3 + 1] = static_cast<uint8_t>(c >> 8);
		palette[i * 3 + 2] = static_cast<uint8_t>(c);
	}
	PlatSetPalette(palette);
	PlatPresent(gVdp.Frame());

	// sound of the frame
	sSamples.clear();
	sSound.RenderTo(gClock.cycles, sSamples);
	AudioPush(sSamples);

	// pace: the sound card sets it (about 50 ms queued); without sound the
	// clock does
	if (sAudioOpen) {
		size_t target;
		uint32_t waited = 0;
		while (AudioFill(&target) > target && waited < 100) {
			PlatSleepMs(2);
			waited += 2;
		}
	} else {
		static double next = 0.0;
		const double frameMs = 1000.0 * kCyclesPerFrame / 3579545.0;
		double now = PlatTicksMs();
		if (next == 0.0)
			next = now;
		next += frameMs;
		if (next > now)
			PlatSleepMs(static_cast<uint32_t>(next - now));
		else if (now - next > 250)
			next = now;
	}
}


void
ResetMachine(const GameImage& image)
{
	InstallGameImage(image);
	memset(PsgRegisters(), 0, 16);
	PsgRegisters()[7] = 0xbf;
	gClock = Clock();
	sSound.Reset();
	Apply(kSetStereo);
	Apply(kSetSmoothSound);
	Apply(kSetNoFlicker);
}

}	// namespace


int
GameMain(int argc, char** argv)
{
	static GameImage image;
	std::string message;
	if (!FindGame(argc, argv, &image, &message)) {
		fprintf(stderr, "%s\n", message.c_str());
		if (!ChooseGame(message, &image))
			return 1;
	}
	printf("Oh Shit!: loaded from a %s\n", image.source.c_str());

	LoadSettings();
	if (sWindowPlace[2] > 0)
		PlatSetSavedWindow(sWindowPlace[0], sWindowPlace[1], sWindowPlace[2], sWindowPlace[3]);
	if (!PlatInit(Tms9918::kWidth, Tms9918::kLines, false, "Oh Shit!"))
		return 1;
	for (int i = 0; i < kSetCount; i++)
		Apply(i);
	sAudioOpen = PlatAudioOpen(PsgSound::kRate, AudioCallback, NULL);

	gHost.frame = EndOfFrame;
	gHost.psgWrite = [](int reg, uint8_t value, uint64_t cycle) {
		sSound.Write(cycle, reg, value);
	};
	for (;;) {
		ResetMachine(image);
		try {
			Start();
		} catch (const QuitGame&) {
			break;
		} catch (const NewGameFile&) {
			image = sNextImage;
			PlatShowMessage(("Game file: " + image.source).c_str());
			continue;
		}
	}
	SaveSettings();
	PlatAudioClose();
	PlatShutdown();
	return 0;
}
