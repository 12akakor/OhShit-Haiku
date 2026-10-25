# Copyright 2026, Akakor. Distributed under the terms of the MIT License.
"""Writes src/game/Symbols.h from tools/dis/names.txt.

gen_symbols.py names.txt blocks.txt Symbols.h

Labels of code (inside a code block of blocks.txt) go to sym::code, so
they do not clash with the C++ routines of the same name; variables,
tables and BIOS addresses are in sym."""
import sys

src, blocks, out = sys.argv[1], sys.argv[2], sys.argv[3]
code = []
for line in open(blocks):
    parts = line.split()
    if len(parts) >= 7 and parts[6] == 'code':
        code.append((int(parts[2], 16), int(parts[4], 16)))


def is_code(a):
    return any(s <= a < e for s, e in code)
lines = []
for line in open(src):
    line = line.strip()
    if not line or line.startswith('#') or line.startswith('='):
        continue
    body, _, com = line.partition(';')
    parts = body.split()
    if len(parts) < 2:
        continue
    lines.append((int(parts[0], 16), parts[1], com.strip()))
with open(out, 'w') as f:
    f.write('/*\n * Copyright 2026, Akakor.\n'
            ' * All rights reserved. Distributed under the terms of the MIT License.\n */\n'
            '#ifndef OS_SYMBOLS_H\n#define OS_SYMBOLS_H\n\n\n'
            '// Addresses of the original program, generated from tools/dis/names.txt\n'
            '// by tools/dis/gen_symbols.py. The port keeps the variables at these\n'
            '// addresses, so every routine can be compared with the original.\n\n'
            '#include <stdint.h>\n\n\nnamespace sym {\n\n')
    for a, n, c in lines:
        if not is_code(a) and 0x8107 <= a < 0xf000:
            f.write('const uint16_t %s = 0x%04x;%s\n' % (n, a, ('\t// ' + c) if c else ''))
    f.write('\n\n// Routines of the original (for Sync and Restart).\nnamespace code {\n\n')
    for a, n, c in lines:
        if is_code(a):
            f.write('const uint16_t %s = 0x%04x;%s\n' % (n, a, ('\t// ' + c) if c else ''))
    f.write('\n}\t// namespace code\n\n\n// BIOS entry points and system variables.\nnamespace system {\n\n')
    for a, n, c in lines:
        if not (0x8107 <= a < 0xf000):
            f.write('const uint16_t %s = 0x%04x;%s\n' % (n, a, ('\t// ' + c) if c else ''))
    f.write('\n}\t// namespace system\n\n}\t// namespace sym\n\n\n#endif\t// OS_SYMBOLS_H\n')
