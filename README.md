# Oh Shit! for Haiku (work in progress)

**Oh Shit!** (Aackosoft, 1986), the MSX1 maze game with digitized speech,
as a native Haiku game: the original program rewritten in C++, checked
against the original with differential tests, like
[Metal Gear for Haiku](https://github.com/12akakor/MetalGear-Haiku).

The game files are not included; the player provides a ROM, cassette
(.cas) or disk (.dsk) image.

## Status

- The whole program rewritten in C++ (`src/game`), checked against the
  original with a differential test (0 failures in over 400000
  comparisons) and running at the original's speed. Loads the cassette,
  disk and ROM versions.
- The Haiku application (`src/main.cpp`, `src/platform`): window with a
  scaled picture (xBR, sharp or whole pixel steps), the sound and speech
  through a cycle-timed AY-3-8910 (`src/audio/PsgSound`), keyboard and
  joystick, settings window, screenshots. It compiles against the Haiku
  headers but has not been run on Haiku yet.
- MSX1 reference machine for the tests (`src/z80`, `src/msx1`): Z80,
  TMS9918A, PSG, PPI, real C-BIOS as the BIOS. Its video memory and game
  state match openMSX with the same BIOS.
- `osrun`: runs the original headless (screenshots, sound with the speech,
  coverage, I/O and BIOS use).
- `tools/dis`: separates code and data from the coverage and produces a
  source that reassembles to the identical program.

## Building

On Haiku or RenkuOS:

    make            # the game, OhShit
    make package    # ohshit-1.0.0-1-<arch>.hpkg

On the first start the game asks for a game file (.rom, .cas or .dsk) and
keeps a copy in its settings folder. Keys: arrows (or a joystick) move,
space starts, P pauses, Esc goes back to the title, Page Down is SELECT
(joystick test). F6 settings, F7 picture, F8 timing, F9 screenshot, F11
full screen, F12 quit.

The test and helper tools build on any POSIX system:

    make tools
    ./osrun game.rom 1800 -shot 900 -wav out.wav
    ./osnative game.rom 1800 -shot 900 -wav out.wav
    make test GAME_FILE=game.rom

C-BIOS is needed for the tests (`/usr/share/cbios`, package `cbios`).

Port by Akakor, developed with the help of Claude (Anthropic). MIT licence
for the code in this repository.
