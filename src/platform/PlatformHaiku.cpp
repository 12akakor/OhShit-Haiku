/*
 * Copyright 2026, Akakor.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


// Native Haiku / RenkuOS back end (Be API, no SDL).
//
// window/video  BApplication, BWindow, BView, BBitmap (B_RGB32)
// keyboard      get_key_info() polling while the window is active
// audio         BSoundPlayer, feeding the portable mixer
// timing        system_time(), snooze()
// files         find_directory(), app directory via app_info
//
// The game runs on its own thread (GameMain()); the BApplication message
// loop runs on the main thread.
#include <atomic>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <string>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Font.h>
#include <GridLayout.h>
#include <Joystick.h>
#include <LayoutBuilder.h>
#include <Locker.h>
#include <Looper.h>
#include <MediaDefs.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <NodeInfo.h>
#include <OS.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Screen.h>
#include <SoundPlayer.h>
#include <String.h>
#include <StringView.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <View.h>
#include <Window.h>

#include "PlatformInternal.h"
#include "VideoScaler.h"


static const char* kSignature = "application/x-vnd.Akakor-OhShit";
static const char* kSettingsName = "OhShit";

enum {
	kMsgRedraw = 'mgRd',
	kMsgSetting = 'mgSt',
	kMsgUpdateSetting = 'mgSU'
};


// Haiku raw key codes (physical keys) -> MOGK_*.
static const MOGKey kRawToMog[128] = {
	/* 0x00 */ 0, MOGK_ESCAPE, MOGK_F1, MOGK_F2, MOGK_F3, MOGK_F4, MOGK_F5,
	/* 0x07 */ MOGK_F6, MOGK_F7, MOGK_F8, MOGK_F9, MOGK_F10, MOGK_F11,
	/* 0x0d */ MOGK_F12, MOGK_PRINT, MOGK_SCROLLOCK, MOGK_PAUSE,
	/* 0x11 */ MOGK_BACKQUOTE, MOGK_1, MOGK_2, MOGK_3, MOGK_4, MOGK_5, MOGK_6,
	/* 0x18 */ MOGK_7, MOGK_8, MOGK_9, MOGK_0, MOGK_MINUS, MOGK_EQUALS,
	/* 0x1e */ MOGK_BACKSPACE, MOGK_INSERT, MOGK_HOME, MOGK_PAGEUP,
	/* 0x22 */ MOGK_NUMLOCK, MOGK_KP_DIVIDE, MOGK_KP_MULTIPLY, MOGK_KP_MINUS,
	/* 0x26 */ MOGK_TAB, MOGK_q, MOGK_w, MOGK_e, MOGK_r, MOGK_t, MOGK_y,
	/* 0x2d */ MOGK_u, MOGK_i, MOGK_o, MOGK_p, MOGK_LEFTBRACKET,
	/* 0x32 */ MOGK_RIGHTBRACKET, MOGK_BACKSLASH, MOGK_DELETE, MOGK_END,
	/* 0x36 */ MOGK_PAGEDOWN, MOGK_KP7, MOGK_KP8, MOGK_KP9, MOGK_KP_PLUS,
	/* 0x3b */ MOGK_CAPSLOCK, MOGK_a, MOGK_s, MOGK_d, MOGK_f, MOGK_g, MOGK_h,
	/* 0x42 */ MOGK_j, MOGK_k, MOGK_l, MOGK_SEMICOLON, MOGK_QUOTE,
	/* 0x47 */ MOGK_RETURN, MOGK_KP4, MOGK_KP5, MOGK_KP6, MOGK_LSHIFT,
	/* 0x4c */ MOGK_z, MOGK_x, MOGK_c, MOGK_v, MOGK_b, MOGK_n, MOGK_m,
	/* 0x53 */ MOGK_COMMA, MOGK_PERIOD, MOGK_SLASH, MOGK_RSHIFT, MOGK_UP,
	/* 0x58 */ MOGK_KP1, MOGK_KP2, MOGK_KP3, MOGK_KP_ENTER, MOGK_LCTRL,
	/* 0x5d */ MOGK_LALT, MOGK_SPACE, MOGK_RALT, MOGK_RCTRL, MOGK_LEFT,
	/* 0x62 */ MOGK_DOWN, MOGK_RIGHT, MOGK_KP0, MOGK_KP_PERIOD, MOGK_LSUPER,
	/* 0x67 */ MOGK_RSUPER, MOGK_MENU, MOGK_LESS, MOGK_KP_EQUALS, MOGK_POWER,
	/* 0x6c */ 0, 0, 0, 0,
	/* 0x70 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


class GameView;
class GameWindow;

namespace {

GameWindow* sWindow = NULL;
GameView* sView = NULL;
std::atomic<bool> sActive(false);
std::atomic<bool> sRedrawPending(false);
std::atomic<int> sViewWidth(0);
std::atomic<int> sViewHeight(0);
std::atomic<bool> sGameDone(false);

int sWidth = 0;
int sHeight = 0;
bool sFullscreen = false;
bool sCursorHidden = false;
BRect sWindowedFrame;
BRect sSavedFrame;
bool sHaveSavedFrame = false;
char sTitle[128] = "";
uint32* sConverted = NULL;

// Front bitmap is drawn by the window thread, back bitmap is written by the
// game thread. sFrameLock protects the swap and the drawing.
BLocker sFrameLock("mg frame");
BBitmap* sFront = NULL;
BBitmap* sBack = NULL;

uint8 sRawKeys[16];

// Fade between rooms (PlatStartTransition): the picture it starts from,
// at the size of the window, and when. Game thread only.
std::vector<uint32> sFromPixels;
int sFromWidth = 0;
int sFromHeight = 0;
bigtime_t sTransitionStart = 0;
bigtime_t sTransitionLength = 0;

// The settings window, once opened (it hides when closed).
BMessenger sSettingsMessenger;

// Vertical sync: wanted (setting) and what the driver supports
// (0 unknown, 1 works, -1 not supported).
std::atomic<bool> sVsyncWanted(true);
std::atomic<int> sVsyncState(0);

// Timing display (F4). The counters are protected by sFrameLock.
std::atomic<bool> sShowStats(false);
struct Stats {
	bigtime_t	start;
	bigtime_t	filterSum;
	bigtime_t	filterMax;
	int			frames;
	bigtime_t	drawSum;
	bigtime_t	drawMax;
	int			draws;
	int			dirtyTiles;
	int			tiles;
};
Stats sStats;
char sStatsText[200] = "";

// Short message at the bottom (e.g. "Screenshot saved"), shown until
// sMessageUntil. Protected by sFrameLock.
char sMessage[B_PATH_NAME_LENGTH + 64] = "";
bigtime_t sMessageUntil = 0;

BLocker sAudioLock("mg audio");
BSoundPlayer* sPlayer = NULL;
PlatAudioCallback sAudioCallback = NULL;
void* sAudioCookie = NULL;
int16_t* sAudioTemp = NULL;
size_t sAudioTempFrames = 0;

char sDataDir[B_PATH_NAME_LENGTH];
char sSettingsDir[B_PATH_NAME_LENGTH];

int sArgc = 0;
char** sArgv = NULL;
thread_id sGameThread = -1;
int sExitCode = 0;

}	// namespace


// View ------------------------------------------------------------------

class GameView : public BView {
public:
	GameView(BRect frame)
		:
		BView(frame, "game", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
		sViewWidth = frame.IntegerWidth() + 1;
		sViewHeight = frame.IntegerHeight() + 1;
	}

	virtual ~GameView()
	{
		delete fScreen;
	}

	virtual void AttachedToWindow()
	{
		MakeFocus(true);
		fScreen = new BScreen(Window());
	}

	virtual void Draw(BRect /*updateRect*/)
	{
		_WaitForRetrace();

		BRect bounds = Bounds();
		sFrameLock.Lock();
		bigtime_t start = system_time();
		if (sFront == NULL) {
			SetHighColor(0, 0, 0);
			FillRect(bounds);
		} else if (sFront->Bounds() == bounds) {
			DrawBitmap(sFront, BPoint(0, 0));
		} else {
			// Size changed, the next frame will match again.
			DrawBitmap(sFront, sFront->Bounds(), bounds);
		}
		if (sShowStats)
			_DrawLabel(sStatsText, false);
		if (sMessage[0] != '\0' && system_time() < sMessageUntil)
			_DrawLabel(sMessage, true);
		// Wait until the app_server has used the bitmap before the game
		// thread may write into it again.
		Sync();
		bigtime_t time = system_time() - start;
		sStats.drawSum += time;
		if (time > sStats.drawMax)
			sStats.drawMax = time;
		sStats.draws++;
		sFrameLock.Unlock();
	}

	virtual void FrameResized(float width, float height)
	{
		sViewWidth = static_cast<int>(width) + 1;
		sViewHeight = static_cast<int>(height) + 1;
		Invalidate();
	}

	// A file dropped on the picture (a ROM): the game decides.
	virtual void MessageReceived(BMessage* message)
	{
		entry_ref ref;
		if (message->WasDropped() && message->FindRef("refs", &ref) == B_OK) {
			BPath path(&ref);
			if (path.InitCheck() == B_OK)
				PlatInternalPushFile(path.Path());
			return;
		}
		BView::MessageReceived(message);
	}

	// The game reads the keyboard state itself; swallow key messages so
	// they do not trigger anything else.
	virtual void KeyDown(const char* /*bytes*/, int32 /*numBytes*/) {}
	virtual void KeyUp(const char* /*bytes*/, int32 /*numBytes*/) {}

private:
	void _WaitForRetrace()
	{
		if (!sVsyncWanted || sVsyncState < 0 || fScreen == NULL)
			return;
		// At most a little longer than one refresh at 50 Hz.
		status_t status = fScreen->WaitForRetrace(25000);
		if (status == B_OK) {
			sVsyncState = 1;
			fTimeouts = 0;
		} else if (status == B_TIMED_OUT) {
			if (++fTimeouts >= 3)
				sVsyncState = -1;
		} else
			sVsyncState = -1;
	}

	// Yellow text on black, top left or bottom left.
	void _DrawLabel(const char* text, bool bottom)
	{
		BFont font(be_fixed_font);
		font.SetSize(13);
		SetFont(&font);
		font_height height;
		font.GetHeight(&height);
		float width = StringWidth(text);
		float lineHeight = ceilf(height.ascent + height.descent);
		float top = bottom ? Bounds().bottom - lineHeight - 10 : 4;
		SetDrawingMode(B_OP_COPY);
		SetHighColor(0, 0, 0);
		FillRect(BRect(4, top, 4 + width + 10, top + lineHeight + 6));
		SetHighColor(255, 220, 0);
		SetLowColor(0, 0, 0);
		DrawString(text, BPoint(9, top + 3 + ceilf(height.ascent)));
	}

	BScreen*	fScreen = NULL;
	int			fTimeouts = 0;
};


// Window ----------------------------------------------------------------

class GameWindow : public BWindow {
public:
	GameWindow(BRect frame, const char* title)
		:
		BWindow(frame, title, B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS)
	{
		sView = new GameView(Bounds());
		AddChild(sView);
	}

	virtual bool QuitRequested()
	{
		// Let the game shut down cleanly; PlatShutdown() closes us.
		PlatInternalPushEvent(PLAT_EV_QUIT, 0, 0);
		return false;
	}

	virtual void WindowActivated(bool active)
	{
		sActive = active;
		BWindow::WindowActivated(active);
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == kMsgRedraw) {
			sRedrawPending = false;
			sView->Invalidate();
			return;
		}
		BWindow::MessageReceived(message);
	}
};


// Application -----------------------------------------------------------

static int32
GameThread(void* /*data*/)
{
	sExitCode = GameMain(sArgc, sArgv);
	sGameDone = true;
	be_app->PostMessage(B_QUIT_REQUESTED);
	return sExitCode;
}


class GameApp : public BApplication {
public:
	GameApp()
		:
		BApplication(kSignature)
	{
	}

	virtual void ReadyToRun()
	{
		sGameThread = spawn_thread(GameThread, "Oh Shit game", B_DISPLAY_PRIORITY,
			NULL);
		if (sGameThread < B_OK || resume_thread(sGameThread) != B_OK) {
			fprintf(stderr, "Cannot start the game thread\n");
			sGameDone = true;
			Quit();
		}
	}

	virtual bool QuitRequested()
	{
		if (!sGameDone) {
			PlatInternalPushEvent(PLAT_EV_QUIT, 0, 0);
			return false;
		}
		return true;
	}

	// A ROM dropped on the program's icon while it runs.
	virtual void RefsReceived(BMessage* message)
	{
		entry_ref ref;
		if (message->FindRef("refs", &ref) == B_OK) {
			BPath path(&ref);
			if (path.InitCheck() == B_OK)
				PlatInternalPushFile(path.Path());
		}
	}
};


// Paths -----------------------------------------------------------------

static void
FindSettingsDir(void)
{
	const char* env = getenv("MG_SETTINGS");
	if (env != NULL) {
		create_directory(env, 0755);
		snprintf(sSettingsDir, sizeof(sSettingsDir), "%s", env);
		return;
	}
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) == B_OK) {
		path.Append(kSettingsName);
		create_directory(path.Path(), 0755);
		snprintf(sSettingsDir, sizeof(sSettingsDir), "%s", path.Path());
	} else {
		snprintf(sSettingsDir, sizeof(sSettingsDir), "/tmp");
	}
}


int
main(int argc, char** argv)
{
	sArgc = argc;
	sArgv = argv;

	GameApp* app = new GameApp();

	FindSettingsDir();
	// The game has no data files of its own: the game file is looked for
	// by the game (main.cpp).
	snprintf(sDataDir, sizeof(sDataDir), "%s", sSettingsDir);
	if (chdir(sDataDir) != 0) {
		fprintf(stderr, "Cannot use data directory %s: %s\n", sDataDir,
			strerror(errno));
		delete app;
		return 1;
	}

	app->Run();
	delete app;

	if (sGameThread >= B_OK) {
		status_t result;
		wait_for_thread(sGameThread, &result);
	}
	return sExitCode;
}


// Joystick -------------------------------------------------------------

static BJoystick* sJoystick = NULL;
static bool sJoystickTried = false;


uint8_t
PlatJoystick(void)
{
	if (!sJoystickTried) {
		sJoystickTried = true;
		BJoystick* stick = new BJoystick;
		char name[B_OS_NAME_LENGTH];
		if (stick->CountDevices() > 0 && stick->GetDeviceName(0, name) == B_OK
			&& stick->Open(name, true) >= 0)
			sJoystick = stick;
		else
			delete stick;
	}
	if (sJoystick == NULL || sJoystick->Update() != B_OK)
		return 0;

	uint8_t bits = 0;
	int32 axes = sJoystick->CountAxes();
	if (axes >= 2) {
		static std::vector<int16> values;
		values.resize(axes);
		if (sJoystick->GetAxisValues(values.data()) != B_OK)
			return 0;
		const int16 kDeadZone = 12000;
		if (values[1] < -kDeadZone)
			bits |= 0x01;
		if (values[1] > kDeadZone)
			bits |= 0x02;
		if (values[0] < -kDeadZone)
			bits |= 0x04;
		if (values[0] > kDeadZone)
			bits |= 0x08;
	}
	uint32 buttons = sJoystick->ButtonValues();
	if ((buttons & 1) != 0)
		bits |= 0x10;
	if ((buttons & 2) != 0)
		bits |= 0x20;
	return bits;
}


const char*
PlatAppDir(void)
{
	static char dir[B_PATH_NAME_LENGTH] = "";
	if (dir[0] == '\0') {
		app_info info;
		BPath path;
		if (be_app != NULL && be_app->GetAppInfo(&info) == B_OK) {
			BEntry entry(&info.ref);
			if (entry.GetPath(&path) == B_OK && path.GetParent(&path) == B_OK)
				snprintf(dir, sizeof(dir), "%s", path.Path());
		}
		if (dir[0] == '\0')
			snprintf(dir, sizeof(dir), ".");
	}
	return dir;
}


void
PlatAlert(const char* title, const char* text)
{
	BAlert* alert = new BAlert(title, text, "OK", NULL, NULL, B_WIDTH_AS_USUAL,
		B_WARNING_ALERT);
	alert->Go();
}


int
PlatAsk(const char* title, const char* text, const char* first, const char* second)
{
	BAlert* alert = new BAlert(title, text, first, second, NULL, B_WIDTH_AS_USUAL,
		B_INFO_ALERT);
	alert->SetShortcut(1, B_ESCAPE);
	return alert->Go() == 0 ? 0 : 1;
}


// Video -----------------------------------------------------------------

static BRect
WindowedFrame(void)
{
	BScreen screen;
	BRect screenFrame = screen.Frame();
	if (sHaveSavedFrame) {
		// Saved placement, if it is still usable on this screen.
		BRect frame = sSavedFrame;
		if (frame.Width() + 1 >= sWidth / 2 && frame.Height() + 1 >= sHeight / 2
			&& frame.Width() <= screenFrame.Width()
			&& frame.Height() <= screenFrame.Height()
			&& frame.top >= screenFrame.top
			&& frame.top <= screenFrame.bottom - 40
			&& frame.left <= screenFrame.right - 80
			&& frame.right >= screenFrame.left + 80)
			return frame;
	}

	// As large as fits comfortably on the screen (90% of the width, 85% of
	// the height), keeping 16:10; at least the game's own size. The
	// scaler fills any window size.
	float maxWidth = (screenFrame.Width() + 1) * 0.90f;
	float maxHeight = (screenFrame.Height() + 1) * 0.85f;
	float width = maxWidth;
	float height = width * sHeight / sWidth;
	if (height > maxHeight) {
		height = maxHeight;
		width = height * sWidth / sHeight;
	}
	if (width < sWidth || height < sHeight) {
		width = sWidth;
		height = sHeight;
	}
	BRect frame(0, 0, static_cast<int>(width) - 1, static_cast<int>(height) - 1);
	frame.OffsetTo(screenFrame.left + static_cast<int>((screenFrame.Width() - frame.Width()) / 2),
		screenFrame.top + static_cast<int>((screenFrame.Height() - frame.Height()) / 2));
	return frame;
}


bool
PlatInit(int width, int height, bool fullscreen, const char* title)
{
	sWidth = width;
	sHeight = height;
	sConverted = new uint32[static_cast<size_t>(width) * height];
	memset(sConverted, 0, sizeof(uint32) * static_cast<size_t>(width) * height);

	snprintf(sTitle, sizeof(sTitle), "%s", title);
	sWindowedFrame = WindowedFrame();
	sWindow = new GameWindow(sWindowedFrame, title);
	PlatBackendScalerChanged();
	sWindow->SetSizeLimits(width / 2, 32768, height / 2, 32768);
	sWindow->Show();

	if (fullscreen)
		PlatSetFullscreen(true);
	return true;
}


void
PlatShutdown(void)
{
	PlatAudioClose();

	if (sCursorHidden) {
		be_app->ShowCursor();
		sCursorHidden = false;
	}
	if (sWindow != NULL && sWindow->Lock()) {
		sWindow->Quit();
		sWindow = NULL;
		sView = NULL;
	}

	sFrameLock.Lock();
	delete sFront;
	delete sBack;
	sFront = sBack = NULL;
	sFrameLock.Unlock();

	delete[] sConverted;
	sConverted = NULL;
}


const char*
PlatName(void)
{
	return "Haiku/RenkuOS native (Be API)";
}


// Called with sFrameLock held, after each frame.
static void
UpdateStats(Scaler* scaler, bigtime_t filterTime, bigtime_t now)
{
	Stats& st = sStats;
	if (st.start == 0)
		st.start = now;
	st.filterSum += filterTime;
	if (filterTime > st.filterMax)
		st.filterMax = filterTime;
	st.frames++;
	if (strcmp(scaler->Name(), "xbr") == 0) {
		XbrScaler* xbr = static_cast<XbrScaler*>(scaler);
		st.dirtyTiles += xbr->LastDirtyTiles();
		st.tiles += xbr->TileCount();
	}
	if (now - st.start < 1000000)
		return;

	double seconds = (now - st.start) / 1000000.0;
	char tiles[32] = "";
	if (st.tiles > 0) {
		snprintf(tiles, sizeof(tiles), " | redrawn %d%%",
			static_cast<int>(100.0 * st.dirtyTiles / st.tiles + 0.5));
	}
	const char* vsync = !sVsyncWanted ? "off"
		: (sVsyncState > 0 ? "on" : (sVsyncState < 0 ? "not supported" : "?"));
	snprintf(sStatsText, sizeof(sStatsText),
		"%s | filter %.1f ms (max %.1f)%s | draw %.1f ms (max %.1f) | "
		"%.0f fps | vsync %s",
		scaler->Name(), st.filterSum / 1000.0 / st.frames,
		st.filterMax / 1000.0, tiles,
		st.draws > 0 ? st.drawSum / 1000.0 / st.draws : 0.0,
		st.drawMax / 1000.0, st.frames / seconds, vsync);
	if (sShowStats) {
		// Flush: with "| tee" stdout is fully buffered and lines would be
		// lost when the game is stopped with Ctrl+C.
		printf("%s\n", sStatsText);
		fflush(stdout);
	}
	memset(&st, 0, sizeof(st));
	st.start = now;
}


void
PlatPresent(const uint8_t* pixels)
{
	if (sWindow == NULL)
		return;

	bigtime_t start = system_time();
	PlatInternalConvert(pixels, sConverted, sWidth * sHeight);

	int width = sViewWidth;
	int height = sViewHeight;
	if (width < 1 || height < 1)
		return;

	if (sBack == NULL || sBack->Bounds().IntegerWidth() + 1 != width
		|| sBack->Bounds().IntegerHeight() + 1 != height) {
		delete sBack;
		sBack = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGB32);
		if (sBack->InitCheck() != B_OK) {
			delete sBack;
			sBack = NULL;
			return;
		}
	}

	Scaler* scaler = PlatInternalScaler();
	uint32_t* bits = reinterpret_cast<uint32_t*>(sBack->Bits());
	int stride = static_cast<int>(sBack->BytesPerRow() / 4);
	scaler->Process(sConverted, sWidth, sHeight, PlatInternalSourceFactor(), bits, width,
		height, stride);
	if (PlatInternalHdTiles() != NULL) {
		int x, y, w, h;
		ScalerPlacement(scaler, sWidth, sHeight, PlatInternalSourceFactor(), width, height,
			&x, &y, &w, &h);
		// a picture larger than the window (nearest) is not drawn in HD
		if (x >= 0 && y >= 0 && x + w <= width && y + h <= height) {
			static HdComposer composer;
			composer.Compose(PlatInternalHdTiles(), PlatInternalHdLocal(), sWidth, sHeight,
				bits, stride, x, y, w, h);
		}
	}
	if (sTransitionLength > 0) {
		bigtime_t elapsed = system_time() - sTransitionStart;
		if (elapsed >= sTransitionLength || sFromWidth != width || sFromHeight != height)
			sTransitionLength = 0;
		else {
			// cross-fade: alpha of the new picture 0-256
			uint32 alpha = static_cast<uint32>(elapsed * 256 / sTransitionLength);
			for (int row = 0; row < height; row++) {
				uint32* out = bits + static_cast<size_t>(row) * stride;
				const uint32* from = sFromPixels.data() + static_cast<size_t>(row) * width;
				for (int i = 0; i < width; i++) {
					uint32 a = from[i], b = out[i];
					uint32 rb = (((a & 0xff00ff) * (256 - alpha) + (b & 0xff00ff) * alpha) >> 8)
						& 0xff00ff;
					uint32 g = (((a & 0x00ff00) * (256 - alpha) + (b & 0x00ff00) * alpha) >> 8)
						& 0x00ff00;
					out[i] = 0xff000000 | rb | g;
				}
			}
		}
	}
	bigtime_t now = system_time();

	sFrameLock.Lock();
	BBitmap* swap = sFront;
	sFront = sBack;
	sBack = swap;
	UpdateStats(scaler, now - start, now);
	sFrameLock.Unlock();

	if (!sRedrawPending.exchange(true))
		sWindow->PostMessage(kMsgRedraw);
}


bool
PlatSetFullscreen(bool fullscreen)
{
	if (sWindow == NULL || fullscreen == sFullscreen)
		return sFullscreen;
	if (!sWindow->Lock())
		return sFullscreen;

	if (fullscreen) {
		sWindowedFrame = sWindow->Frame();
		BScreen screen(sWindow);
		BRect frame = screen.Frame();
		sWindow->SetLook(B_NO_BORDER_WINDOW_LOOK);
		sWindow->SetFeel(B_FLOATING_ALL_WINDOW_FEEL);
		sWindow->MoveTo(frame.LeftTop());
		sWindow->ResizeTo(frame.Width(), frame.Height());
		sWindow->Activate(true);
		if (!sCursorHidden) {
			be_app->HideCursor();
			sCursorHidden = true;
		}
	} else {
		sWindow->SetFeel(B_NORMAL_WINDOW_FEEL);
		sWindow->SetLook(B_TITLED_WINDOW_LOOK);
		sWindow->MoveTo(sWindowedFrame.LeftTop());
		sWindow->ResizeTo(sWindowedFrame.Width(), sWindowedFrame.Height());
		if (sCursorHidden) {
			be_app->ShowCursor();
			sCursorHidden = false;
		}
	}
	sFullscreen = fullscreen;
	sWindow->Unlock();
	return sFullscreen;
}


void
PlatBackendScalerChanged(void)
{
	if (sWindow == NULL || !sWindow->Lock())
		return;
	char title[160];
	snprintf(title, sizeof(title), "%s [%s]", sTitle, PlatScalerName());
	sWindow->SetTitle(title);
	sWindow->Unlock();
}


bool
PlatIsFullscreen(void)
{
	return sFullscreen;
}


// Shows a message for a few seconds (any thread).
static void
ShowMessage(const char* text)
{
	sFrameLock.Lock();
	snprintf(sMessage, sizeof(sMessage), "%s", text);
	sMessageUntil = system_time() + 3000000;
	sFrameLock.Unlock();
	printf("%s\n", text);
	fflush(stdout);
}


void
PlatShowMessage(const char* text)
{
	ShowMessage(text);
}


void
PlatStartTransition(int ms)
{
	sFrameLock.Lock();
	if (sFront != NULL && ms > 0) {
		sFromWidth = sFront->Bounds().IntegerWidth() + 1;
		sFromHeight = sFront->Bounds().IntegerHeight() + 1;
		sFromPixels.resize(static_cast<size_t>(sFromWidth) * sFromHeight);
		const uint8* src = reinterpret_cast<const uint8*>(sFront->Bits());
		for (int row = 0; row < sFromHeight; row++) {
			memcpy(sFromPixels.data() + static_cast<size_t>(row) * sFromWidth,
				src + static_cast<size_t>(row) * sFront->BytesPerRow(),
				static_cast<size_t>(sFromWidth) * 4);
		}
		sTransitionStart = system_time();
		sTransitionLength = static_cast<bigtime_t>(ms) * 1000;
	}
	sFrameLock.Unlock();
}


// Writes the bitmap as PNG into the home folder; owns the bitmap. Runs on
// its own thread so the game does not wait for the encoding.
static int32
ScreenshotThread(void* data)
{
	BBitmap* bitmap = static_cast<BBitmap*>(data);
	BPath folder;
	status_t status = find_directory(B_USER_DIRECTORY, &folder);

	BPath path;
	if (status == B_OK) {
		time_t now = time(NULL);
		struct tm local;
		localtime_r(&now, &local);
		char stamp[64];
		strftime(stamp, sizeof(stamp), "OhShit %Y-%m-%d %H.%M.%S",
			&local);
		for (int n = 1; n < 100; n++) {
			BString name(stamp);
			if (n > 1)
				name << " " << n;
			name << ".png";
			path.SetTo(folder.Path(), name.String());
			if (!BEntry(path.Path()).Exists())
				break;
		}
	}

	BBitmapStream stream(bitmap);
	if (status == B_OK) {
		BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status = file.InitCheck();
		if (status == B_OK) {
			status = BTranslatorRoster::Default()->Translate(&stream, NULL,
				NULL, &file, B_PNG_FORMAT);
		}
		if (status == B_OK)
			BNodeInfo(&file).SetType("image/png");
	}
	BBitmap* detached = NULL;
	stream.DetachBitmap(&detached);
	delete detached;

	char text[B_PATH_NAME_LENGTH + 64];
	if (status == B_OK)
		snprintf(text, sizeof(text), "Screenshot saved: %s", path.Path());
	else {
		if (path.Path() != NULL)
			unlink(path.Path());
		snprintf(text, sizeof(text), "Screenshot failed: %s",
			strerror(status));
	}
	ShowMessage(text);
	return 0;
}


void
PlatSaveScreenshot(void)
{
	sFrameLock.Lock();
	if (sFront == NULL) {
		sFrameLock.Unlock();
		return;
	}
	int width = sFront->Bounds().IntegerWidth() + 1;
	int height = sFront->Bounds().IntegerHeight() + 1;

	// Only the picture, not the black borders.
	int x, y, w, h;
	if (strcmp(PlatScalerName(), "nearest") == 0) {
		int factor;
		NearestScaler::Layout(sWidth, sHeight, width, height, &factor, &x, &y);
		w = sWidth * factor;
		h = sHeight * factor;
		if (x < 0) {
			w += 2 * x;
			x = 0;
		}
		if (y < 0) {
			h += 2 * y;
			y = 0;
		}
		w = w < width - x ? w : width - x;
		h = h < height - y ? h : height - y;
	} else
		ScalerFit(sWidth, sHeight, width, height, &x, &y, &w, &h);

	BBitmap* shot = new BBitmap(BRect(0, 0, w - 1, h - 1), B_RGB32);
	if (shot->InitCheck() == B_OK) {
		const uint8* src = reinterpret_cast<const uint8*>(sFront->Bits());
		uint8* dst = reinterpret_cast<uint8*>(shot->Bits());
		int32 srcRow = sFront->BytesPerRow();
		int32 dstRow = shot->BytesPerRow();
		for (int row = 0; row < h; row++) {
			memcpy(dst + static_cast<size_t>(row) * dstRow,
				src + static_cast<size_t>(y + row) * srcRow + static_cast<size_t>(x) * 4,
				static_cast<size_t>(w) * 4);
		}
	}
	sFrameLock.Unlock();

	if (shot->InitCheck() != B_OK) {
		delete shot;
		ShowMessage("Screenshot failed: out of memory");
		return;
	}
	thread_id thread = spawn_thread(ScreenshotThread, "Oh Shit screenshot",
		B_LOW_PRIORITY, shot);
	if (thread < B_OK || resume_thread(thread) != B_OK) {
		delete shot;
		ShowMessage("Screenshot failed: cannot start a thread");
	}
}


void
PlatToggleStats(void)
{
	sShowStats = !sShowStats;
	if (sShowStats)
		printf("Timing display on (F4 to hide)\n");
	fflush(stdout);
	if (sWindow != NULL && sWindow->Lock()) {
		sView->Invalidate();
		sWindow->Unlock();
	}
}


void
PlatSetVsync(bool enabled)
{
	sVsyncWanted = enabled;
}


bool
PlatVsync(void)
{
	return sVsyncWanted;
}


void
PlatSetSavedWindow(int x, int y, int width, int height)
{
	if (width < 1 || height < 1)
		return;
	sSavedFrame = BRect(x, y, x + width - 1, y + height - 1);
	sHaveSavedFrame = true;
}


bool
PlatGetWindow(int* x, int* y, int* width, int* height)
{
	if (sWindow == NULL || !sWindow->Lock())
		return false;
	BRect frame = sFullscreen ? sWindowedFrame : sWindow->Frame();
	sWindow->Unlock();
	*x = static_cast<int>(frame.left);
	*y = static_cast<int>(frame.top);
	*width = frame.IntegerWidth() + 1;
	*height = frame.IntegerHeight() + 1;
	return true;
}


// Time ------------------------------------------------------------------

uint32_t
PlatTicksMs(void)
{
	static bigtime_t sStart = system_time();
	return static_cast<uint32_t>((system_time() - sStart) / 1000);
}


void
PlatSleepMs(uint32_t ms)
{
	snooze(static_cast<bigtime_t>(ms) * 1000);
}


// Input -----------------------------------------------------------------

static void
PollKeyboard(void)
{
	if (PlatInternalInputOverride())
		return;

	key_info info;
	if (!sActive || get_key_info(&info) != B_OK) {
		// Window inactive: release everything.
		if (memcmp(sRawKeys, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0) {
			memset(sRawKeys, 0, sizeof(sRawKeys));
			PlatInternalClearKeys();
		}
		return;
	}

	MOGKey pressed[128];
	int pressedCount = 0;
	for (int raw = 1; raw < 128; raw++) {
		uint8 mask = static_cast<uint8>(1 << (7 - (raw & 7)));
		bool down = (info.key_states[raw >> 3] & mask) != 0;
		bool was = (sRawKeys[raw >> 3] & mask) != 0;
		if (down == was)
			continue;
		MOGKey key = kRawToMog[raw];
		if (key == 0)
			continue;
		PlatInternalSetKey(key, down);
		if (down)
			pressed[pressedCount++] = key;
	}
	memcpy(sRawKeys, info.key_states, sizeof(sRawKeys));

	// Events after all keys are updated, so modifiers are current.
	int modifiers = PlatModifiers();
	for (int i = 0; i < pressedCount; i++)
		PlatInternalPushEvent(PLAT_EV_KEYDOWN, pressed[i], modifiers);
}


bool
PlatPollEvent(PlatEvent* event)
{
	PollKeyboard();
	return PlatInternalPopEvent(event);
}


void
PlatPumpEvents(void)
{
	PollKeyboard();
}


// Files -----------------------------------------------------------------

const char*
PlatDataDir(void)
{
	return sDataDir;
}


const char*
PlatSettingsDir(void)
{
	return sSettingsDir;
}


// Audio -----------------------------------------------------------------

static void
PlayerCallback(void* /*cookie*/, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	sAudioLock.Lock();
	if (sAudioCallback == NULL || format.channel_count != 2) {
		memset(buffer, 0, size);
	} else if (format.format == media_raw_audio_format::B_AUDIO_SHORT) {
		sAudioCallback(reinterpret_cast<int16_t*>(buffer), static_cast<int>(size / 4),
			sAudioCookie);
	} else if (format.format == media_raw_audio_format::B_AUDIO_FLOAT) {
		size_t frames = size / 8;
		if (frames > sAudioTempFrames) {
			delete[] sAudioTemp;
			sAudioTemp = new int16_t[frames * 2];
			sAudioTempFrames = frames;
		}
		sAudioCallback(sAudioTemp, static_cast<int>(frames), sAudioCookie);
		float* out = reinterpret_cast<float*>(buffer);
		for (size_t i = 0; i < frames * 2; i++)
			out[i] = sAudioTemp[i] / 32768.0f;
	} else {
		memset(buffer, 0, size);
	}
	sAudioLock.Unlock();
}


bool
PlatAudioOpen(int rate, PlatAudioCallback callback, void* cookie)
{
	media_raw_audio_format format;
	memset(&format, 0, sizeof(format));
	format.frame_rate = static_cast<float>(rate);
	format.channel_count = 2;
	format.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = 2048 * 2 * sizeof(int16_t);

	sAudioLock.Lock();
	sAudioCallback = callback;
	sAudioCookie = cookie;
	sAudioLock.Unlock();

	sPlayer = new BSoundPlayer(&format, "Oh Shit", PlayerCallback);
	if (sPlayer->InitCheck() != B_OK || sPlayer->Start() != B_OK) {
		fprintf(stderr, "BSoundPlayer: %s\n", strerror(sPlayer->InitCheck()));
		delete sPlayer;
		sPlayer = NULL;
		sAudioLock.Lock();
		sAudioCallback = NULL;
		sAudioLock.Unlock();
		return false;
	}
	media_raw_audio_format actual = sPlayer->Format();
	if (static_cast<int>(actual.frame_rate) != rate || actual.channel_count != 2) {
		fprintf(stderr, "Audio: device format %.0f Hz, %u channels is not "
			"supported\n", actual.frame_rate, static_cast<unsigned>(actual.channel_count));
	}
	sPlayer->SetHasData(true);
	return true;
}


void
PlatAudioClose(void)
{
	if (sPlayer == NULL)
		return;
	sPlayer->Stop();
	delete sPlayer;
	sPlayer = NULL;
	sAudioLock.Lock();
	sAudioCallback = NULL;
	delete[] sAudioTemp;
	sAudioTemp = NULL;
	sAudioTempFrames = 0;
	sAudioLock.Unlock();
}


void
PlatAudioLock(void)
{
	sAudioLock.Lock();
}


void
PlatAudioUnlock(void)
{
	sAudioLock.Unlock();
}


// Settings window ----------------------------------------------------------

namespace {

class SettingsWindow : public BWindow {
public:
	SettingsWindow(const PlatSetting* settings, int count, const char* help)
		:
		BWindow(BRect(0, 0, 100, 100), "Oh Shit settings", B_TITLED_WINDOW,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
				| B_ASYNCHRONOUS_CONTROLS | B_CLOSE_ON_ESCAPE)
	{
		// The grid goes into the window first: views added to a layout
		// that has no view yet are never shown.
		BLayoutBuilder::Group<> builder(this, B_VERTICAL, B_USE_DEFAULT_SPACING);
		builder.SetInsets(B_USE_WINDOW_SPACING);
		BGridLayout* grid = NULL;
		builder.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.GetLayout(&grid)
		.End();
		for (int i = 0; i < count; i++) {
			const PlatSetting& setting = settings[i];
			if (setting.choices == NULL) {
				BMessage* message = new BMessage(kMsgSetting);
				message->AddInt32("setting", i);
				BCheckBox* box = new BCheckBox(setting.label, message);
				box->SetValue(setting.value != 0 ? B_CONTROL_ON : B_CONTROL_OFF);
				box->SetEnabled(setting.enabled);
				grid->AddView(box, 1, i);
				fControls.push_back(box);
				fMenus.push_back(NULL);
			} else {
				BPopUpMenu* menu = new BPopUpMenu("choice");
				for (int c = 0; c < setting.count; c++) {
					BMessage* choice = new BMessage(kMsgSetting);
					choice->AddInt32("setting", i);
					choice->AddInt32("value", c);
					BMenuItem* item = new BMenuItem(setting.choices[c], choice);
					item->SetMarked(c == setting.value);
					menu->AddItem(item);
				}
				BMenuField* field = new BMenuField(setting.label, menu);
				field->SetEnabled(setting.enabled);
				grid->AddItem(field->CreateLabelLayoutItem(), 0, i);
				grid->AddItem(field->CreateMenuBarLayoutItem(), 1, i);
				fControls.push_back(NULL);
				fMenus.push_back(menu);
			}
		}

		if (help != NULL && help[0] != '\0') {
			BString text(help);
			int32 start = 0;
			while (start <= text.Length()) {
				int32 end = text.FindFirst('\n', start);
				if (end < 0)
					end = text.Length();
				BString line;
				text.CopyInto(line, start, end - start);
				BStringView* view = new BStringView("help", line.String());
				view->SetFont(be_plain_font);
				builder.Add(view);
				start = end + 1;
			}
		}
		if (sWindow != NULL)
			CenterIn(sWindow->Frame());
		else
			CenterOnScreen();
	}

	virtual bool QuitRequested()
	{
		// Closing hides it; it goes when the program ends.
		if (sGameDone)
			return true;
		Hide();
		return false;
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgSetting:
			{
				int32 setting, value;
				if (message->FindInt32("setting", &setting) != B_OK)
					return;
				if (message->FindInt32("value", &value) != B_OK) {
					BControl* control = NULL;
					if (message->FindPointer("source",
							reinterpret_cast<void**>(&control)) != B_OK || control == NULL)
						return;
					value = control->Value() == B_CONTROL_ON ? 1 : 0;
				}
				PlatInternalPushSetting(setting, value);
				return;
			}
			case kMsgUpdateSetting:
			{
				int32 setting, value;
				if (message->FindInt32("setting", &setting) != B_OK
					|| message->FindInt32("value", &value) != B_OK || setting < 0
					|| setting >= static_cast<int32>(fControls.size()))
					return;
				if (fControls[setting] != NULL)
					fControls[setting]->SetValue(value != 0 ? B_CONTROL_ON : B_CONTROL_OFF);
				else if (fMenus[setting] != NULL) {
					BMenuItem* item = fMenus[setting]->ItemAt(value);
					if (item != NULL)
						item->SetMarked(true);
				}
				return;
			}
		}
		BWindow::MessageReceived(message);
	}

private:
	std::vector<BCheckBox*>	fControls;
	std::vector<BPopUpMenu*> fMenus;
};


// Waits for the open panel: the chosen file, or nothing.
class FileWaiter : public BLooper {
public:
	FileWaiter()
		:
		BLooper("Oh Shit file panel"),
		fDone(create_sem(0, "Oh Shit file chosen")),
		fReleased(false)
	{
	}

	virtual ~FileWaiter()
	{
		delete_sem(fDone);
	}

	virtual void MessageReceived(BMessage* message)
	{
		entry_ref ref;
		if (message->what == B_REFS_RECEIVED && message->FindRef("refs", &ref) == B_OK) {
			BPath path(&ref);
			if (path.InitCheck() == B_OK)
				fPath = path.Path();
			_Release();
			return;
		}
		if (message->what == B_CANCEL) {
			_Release();
			return;
		}
		BLooper::MessageReceived(message);
	}

	sem_id Done() const { return fDone; }
	const BString& Path() const { return fPath; }

private:
	void _Release()
	{
		if (!fReleased) {
			fReleased = true;
			release_sem(fDone);
		}
	}

	sem_id	fDone;
	bool	fReleased;
	BString	fPath;
};

}	// namespace


void
PlatShowSettings(const PlatSetting* settings, int count, const char* help)
{
	if (sSettingsMessenger.IsValid()) {
		BLooper* looper;
		BWindow* window = dynamic_cast<BWindow*>(sSettingsMessenger.Target(&looper));
		if (window != NULL && window->Lock()) {
			if (window->IsHidden())
				window->Show();
			window->Activate();
			window->Unlock();
			return;
		}
	}
	SettingsWindow* window = new SettingsWindow(settings, count, help);
	sSettingsMessenger = BMessenger(window);
	window->Show();
}


void
PlatUpdateSetting(int index, int value)
{
	if (!sSettingsMessenger.IsValid())
		return;
	BMessage message(kMsgUpdateSetting);
	message.AddInt32("setting", index);
	message.AddInt32("value", value);
	sSettingsMessenger.SendMessage(&message);
}


bool
PlatChooseFile(const char* title, char* path, size_t size)
{
	FileWaiter* waiter = new FileWaiter;
	waiter->Run();
	BMessenger target(waiter);
	BFilePanel* panel = new BFilePanel(B_OPEN_PANEL, &target, NULL, B_FILE_NODE, false);
	if (panel->Window()->Lock()) {
		panel->Window()->SetTitle(title);
		panel->Window()->Unlock();
	}
	panel->Show();
	while (acquire_sem(waiter->Done()) == B_INTERRUPTED)
		;
	delete panel;
	bool chosen = waiter->Path().Length() > 0;
	if (chosen)
		snprintf(path, size, "%s", waiter->Path().String());
	if (waiter->Lock())
		waiter->Quit();
	return chosen;
}
