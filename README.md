# Oh Shit! for Haiku (work in progress)

**Oh Shit!** (Aackosoft, 1986), the MSX1 maze game with digitized speech,
as a native Haiku game: the original program rewritten in C++, checked
against the original with differential tests, like
[Metal Gear for Haiku](https://github.com/12akakor/MetalGear-Haiku).

The game files are not included; the player provides a ROM, cassette
(.cas) or disk (.dsk) image.

## Status

- MSX1 reference machine for the tests (`src/z80`, `src/msx1`): Z80,
  TMS9918A, PSG, PPI, real C-BIOS as the BIOS. Its video memory and game
  state match openMSX with the same BIOS.
- `osrun`: runs the original headless (screenshots, sound with the speech,
  coverage, I/O and BIOS use).
- `tools/dis`: separates code and data from the coverage and produces a
  source that reassembles to the identical program.

## Building the tools

    make
    ./osrun game.rom 1800 -shot 900 -wav out.wav

C-BIOS is needed for the tests (`/usr/share/cbios`, package `cbios`).

Port by Akakor, developed with the help of Claude (Anthropic). MIT licence
for the code in this repository.
