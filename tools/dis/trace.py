# Copyright 2026, Akakor. Distributed under the terms of the MIT License.
"""Separates code and data of the game image.

trace.py image.bin origin coverage.bin out_blocks.txt

image.bin: the program as loaded (from origin on). coverage.bin: from
osrun -cov (bit 0 = executed instruction start). Code = executed
instructions plus what is statically reachable from them through
conditional branches and jumps; everything else is data. Writes a block
file for z80dasm and prints statistics."""
import sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from z80len import flow

img = open(sys.argv[1], 'rb').read()
org = int(sys.argv[2], 0)
cov = open(sys.argv[3], 'rb').read()
end = org + len(img)
mem = bytearray(65536)
mem[org:end] = img

start = {}          # instruction start -> length ('x' executed, 's' static)
owner = [None] * 65536


def add(a, kind):
    if a in start:
        return True
    n, _, _ = flow(mem, a)
    for k in range(n):
        o = owner[a + k]
        if o is not None:
            return False        # overlaps another instruction
    for k in range(n):
        owner[a + k] = a
    start[a] = (n, kind)
    return True


execd = [a for a in range(org, end) if cov[a] & 1]
for a in execd:
    if not add(a, 'x'):
        print('conflict at executed %04X' % a)

queue = list(execd)
conflicts = 0
while queue:
    a = queue.pop()
    n, targets, falls = flow(mem, a)
    nxt = []
    if falls:
        nxt.append(a + n)
    nxt += targets
    for t in nxt:
        if not (org <= t < end) or t in start:
            continue
        if cov[t] & 4:          # written at run time: data
            continue
        if add(t, 's'):
            queue.append(t)
        else:
            conflicts += 1

# Calls whose return address is never executed may be followed by inline
# data; that is handled by hand later.
code = sorted(start)
nx = sum(1 for a in code if start[a][1] == 'x')
print('instructions: %d executed, %d static, %d conflicts' % (nx, len(code) - nx, conflicts))

# Ranges
blocks = []
a = org
while a < end:
    is_code = owner[a] is not None
    b = a
    while b < end and (owner[b] is not None) == is_code:
        b += 1
    blocks.append((a, b, is_code))
    a = b
with open(sys.argv[4], 'w') as f:
    for i, (a, b, c) in enumerate(blocks):
        f.write('%s%04x: start 0x%04x end 0x%04x type %s\n'
                % ('c' if c else 'd', a, a, b, 'code' if c else 'bytedata'))
print('code bytes %d, data bytes %d, blocks %d'
      % (sum(b - a for a, b, c in blocks if c), sum(b - a for a, b, c in blocks if not c), len(blocks)))
