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
- **Random numbers** in the demo come from bytes at #5000-#58FF of page 1,
  so from the BIOS (cassette, disk) or the cartridge (ROM version).

## 25 October 2026: the whole program in C++

- Every routine of the original is rewritten in C++ (`src/game`), after
  the disassembly, with the original's names. The variables stay at their
  addresses in one 64 KB array, so each routine can be compared with the
  original. Quirks are kept where they change the state: the address
  WithDi stores in its own CALL, the blue ghosts negating the byte after
  their target, the ghost reverse that always gives the same direction.
- The BIOS calls the program makes are rewritten after C-BIOS (the BIOS of
  the reference machine): VDP and PSG effects and the interrupt state they
  leave.
- **Differential test** `game_diff`: the original on the reference machine
  and the rewrite run in step; at every pass of the main loop (and the
  title, the name entry, the pause) RAM #8107-#EFFF, JIFFY, VRAM, VDP and
  PSG registers and the interrupt flag are compared. Interrupts happen at
  those points on both sides; LD A,R values of the original are replayed.
  With random play, jumps to the end of a maze (`-warp`) and moves of the
  player or ghosts onto fruit, pills and each other (`-poke`): 0 failures
  in over 400000 comparisons, through games, demo, name entry, pause,
  levels up to 11 and both intermissions. Line coverage of the rewrite:
  95 to 100 percent per file, apart from the joystick test.
- **Speed**: the original has no fixed frame rate; a pass of the main loop
  takes its work plus a busy wait that gets shorter every maze. The
  rewrite charges the CPU cycles of the original for every routine
  (measured with `osprof`, per path where the cost differs), the exact
  cycles of the busy waits and of the speech loop. Measured against the
  original: normal passes within 0.1 percent, everything (deaths,
  intermissions) within 0.3 percent.
- **Game files**: all nine known files load (`load_test`): the Aackosoft
  cassette and its File-Hunter copy, the disk, the three ROM conversions,
  and the ByteBusters cassettes (their rebuilt program block: only the
  tables are taken, at their own places). Checked with checksums of the
  code and data of the original.

## Next steps

1. Haiku application: window, sound (PSG and speech), keys and joystick,
   settings, package.
2. Improvements, after the example of the Metal Gear port.
