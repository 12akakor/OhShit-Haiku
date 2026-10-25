# Progress of the Oh Shit! port

## 25 October 2026: reference machine and disassembly

**Versions.** The nine known files (cassette images, ROM conversions, a disk
image and the ByteBusters re-release) all load the same program into RAM:
#8107-#A391 and #A500-#EFFF. The original Aackosoft cassette is the
reference; the ROM conversion "7372" and the disk image are identical to
it. Two other ROM conversions carry 198 changed bytes and a played high
score table; the ByteBusters release has its own loader and a relocated
code block (not analysed yet).

**MSX1 reference machine** (`src/msx1`): the Z80 and PSG from the Metal
Gear port, a new TMS9918A, the slot layout of the C-BIOS MSX1 machine of
openMSX, and the real C-BIOS as the BIOS (nothing emulated in C++). Compared
with openMSX 19.1 after 8 seconds: video memory identical, the program and
its variables identical; only the power on RAM contents and JIFFY differ.

**osrun**: runs the original without a window (screenshots, WAV with the
speech, PSG log, coverage, I/O and BIOS use, random play, calling a
routine).

**Disassembly** (`tools/dis`; the source itself stays private): code and
data separated with the coverage of 6 x 60000 frames of random play plus
static tracing of branches (2923 executed and 637 statically found
instructions, no conflicts). It reassembles byte for byte with Sjasm 0.39j.

## What the program does

- About 3500 instructions of code, the rest data (speech, characters, maze,
  sprites).
- SCREEN 1 (32x24 characters), 16x16 sprites. Its own VDP routines; the
  BIOS only for INIT32, CLRSPR, WRTPSG/RDPSG, GTSTCK, GTTRIG, SNSMAT, and in
  the joystick test CHPUT, CLS, INITXT, LDIRVM, WRTVRM.
- **Main loop:** player, ghosts, sound, ESC, pause (P), fruit, end of
  level, then a **busy wait** whose length drops by 2 every level. The game
  speed therefore depends on CPU time; the port has to reproduce the time
  each pass of the original takes.
- **Interrupt:** replaces the BIOS handler through H.KEYI: animation of the
  player and ghosts and a rotating sprite order (7 sprites, against the
  limit of 4 per line).
- **Speech:** 8 samples, 1 bit: every byte is a duration, the volume of
  channel C toggles between 15 and 0. Interrupts are off while speaking
  (the game stands still).
- **Random numbers** in the demo come from bytes at #5000-#5007 of page 1,
  so from the BIOS (cassette, disk) or the cartridge (ROM version).

## Next steps

1. Finish names and comments (ghosts, fruit, intermissions).
2. Rewrite in C++ routine by routine, with differential tests as in the
   Metal Gear port.
3. Speech with the same timing model as the original loop.
4. Loading from ROM, CAS and DSK (recognising all nine files).
