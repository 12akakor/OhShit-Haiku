# Copyright 2026, Akakor. Distributed under the terms of the MIT License.
"""Turns z80dasm output (-l -a, with a block file) into a readable Sjasm
source with our own labels.

fmt.py raw.asm image.bin origin names.txt out.asm

names.txt: lines "ADDR NAME [; comment]" (ADDR in hex), "ADDR ; comment"
for a comment only, and "=ADDR" for an instruction whose 16 bit operand
is a number, not an address (a delay count, say). Other referenced
addresses get the label Lxxxx. References into the middle of an
instruction or data item become label+offset. Data is grouped into lines
of up to 16 bytes, broken at labels."""
import re
import sys

raw = open(sys.argv[1]).read().splitlines()
img = open(sys.argv[2], 'rb').read()
org = int(sys.argv[3], 0)
end = org + len(img)
names = {}
comments = {}
constSites = set()
try:
    for line in open(sys.argv[4]):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('='):
            constSites.add(int(line[1:].split()[0], 16))
            continue
        body, _, com = line.partition(';')
        parts = body.split()
        a = int(parts[0], 16)
        if len(parts) > 1:
            names[a] = parts[1]
        if com.strip():
            comments[a] = com.strip()
except FileNotFoundError:
    pass

# 1. Items: (addr, kind, text)
items = []
labelAddr = {}      # z80dasm label -> address
pending = []
for line in raw:
    m = re.match(r'^(\w+):\s*equ\s+0x([0-9a-f]+)', line)
    if m:
        labelAddr[m.group(1)] = int(m.group(2), 16)
        continue
    m = re.match(r'^(\w+):\s*$', line)
    if m:
        pending.append(m.group(1))
        continue
    m = re.match(r'^\t(.*?)\s*;([0-9a-f]{4})$', line)
    if m:
        a = int(m.group(2), 16)
        for p in pending:
            labelAddr[p] = a
        pending = []
        text = m.group(1).strip()
        kind = 'data' if text.startswith('defb') else 'code'
        items.append([a, kind, text])
for p in pending:
    labelAddr[p] = end
items.sort()
starts = [it[0] for it in items]
itemAt = {it[0]: it for it in items}

import bisect


def containing(a):
    i = bisect.bisect_right(starts, a) - 1
    return starts[i] if i >= 0 else None


def label_name(a):
    if a in names:
        return names[a]
    return 'L%04X' % a


referenced = set(names)


def symbol(a):
    """Label text for address a (inside the image)."""
    if a == end:
        referenced.add(a)
        return label_name(a)
    s = containing(a)
    if s is None:
        return None
    # data items are single bytes, so this only offsets into code
    referenced.add(s)
    return label_name(s) if s == a else '%s+%d' % (label_name(s), a - s)


def hexnum(m):
    v = int(m.group(1), 16)
    return '#%0*X' % (2 if len(m.group(1)) <= 3 and v < 256 else 4, v)


ADDR_OPS = re.compile(r'^(jp|jr|call|djnz|ld|cp|add|sub|and|or|xor)\b')


def rewrite(text):
    # z80dasm labels to addresses
    def lab(m):
        name = m.group(0)
        if name in labelAddr:
            return '@%04X' % labelAddr[name]
        mm = re.fullmatch(r'(?:l|sub_)([0-9a-f]{4})h', name)
        if mm:
            return '@%04X' % int(mm.group(1), 16)
        return name
    text = re.sub(r'\b(?:[cdl][0-9a-f]{4}(?:h|_start|_end)|sub_[0-9a-f]{4}h)\b', lab, text)
    # 16 bit numbers in the image range
    def num(m):
        v = int(m.group(1), 16)
        if len(m.group(1)) >= 4 and org <= v <= end and ADDR_OPS.match(text):
            return '@%04X' % v
        return m.group(0)
    text = re.sub(r'\b0?([0-9a-f]+)h\b', num, text)
    # names outside the image: BIOS entry points and system variables
    def ext(m):
        v = int(m.group(2), 16)
        if v in names and not (org <= v <= end):
            externals.add(v)
            return m.group(1) + names[v] + m.group(3)
        return m.group(0)
    if re.match(r'^(call|jp|rst)\b', text):
        text = re.sub(r'(\s|,)0?([0-9a-f]+)h()$', ext, text)
    text = re.sub(r'(\()0?([0-9a-f]{4,5})h(\))', ext, text)
    if re.match(r'^ld\s+(bc|de|hl|sp|ix|iy),', text):
        text = re.sub(r'(,)0?([0-9a-f]{4,5})h()$',
                      lambda m: ext(m) if int(m.group(2), 16) >= 0xf000 else m.group(0), text)
    return text


externals = set()


for it in items:
    if it[1] == 'code':
        if it[0] in constSites:
            it[2] = re.sub(r'\b(?:[cdl][0-9a-f]{4}(?:h|_start|_end)|sub_[0-9a-f]{4}h)\b',
                           lambda m: '%04xh' % labelAddr.get(m.group(0), 0)
                           if m.group(0) in labelAddr else '0' + m.group(0)[1:5] + 'h'
                           if m.group(0).startswith('l') else m.group(0), it[2])
        else:
            it[2] = rewrite(it[2])


def finish(text):
    text = re.sub(r'@([0-9A-F]{4})', lambda m: symbol(int(m.group(1), 16)), text)
    text = re.sub(r'\b0?([0-9a-f]+)h\b', hexnum, text)
    return text


# resolve symbols first (fills 'referenced')
for it in items:
    if it[1] == 'code':
        it[2] = finish(it[2])

out = ['; Oh Shit! (Aackosoft, 1986), program image #%04X-#%04X' % (org, end - 1),
       '; Generated by tools/dis; names and comments from names.txt.',
       '',
       '']
for a in sorted(externals):
    out.append('%s\tequ #%04X%s' % (names[a], a,
               ('\t; ' + comments[a]) if a in comments else ''))
out += ['', '\torg #%04X' % org, '']
i = 0
while i < len(items):
    a, kind, text = items[i]
    if a in referenced:
        if a in comments and kind == 'code' and a in names:
            out.append('')
            out.append('; ' + comments[a])
        out.append(label_name(a) + ':')
    if kind == 'code':
        line = '\t' + text.replace(' ', '\t', 1) if ' ' in text else '\t' + text
        line = line.rstrip()
        if a in comments and a not in names:
            line += '\t\t; ' + comments[a]
        out.append(line)
        i += 1
        continue
    # data run
    vals = []
    j = i
    while j < len(items) and items[j][1] == 'data' and len(vals) < 16:
        if j > i and items[j][0] in referenced:
            break
        vals.append(img[items[j][0] - org])
        j += 1
    line = '\tdb ' + ','.join('#%02X' % v for v in vals)
    if a in comments and a not in names:
        line += '\t; ' + comments[a]
    out.append(line)
    i = j
if end in referenced:
    out.append(label_name(end) + ':')
open(sys.argv[5], 'w').write('\n'.join(out) + '\n')
print('%d lines, %d labels' % (len(out), len(referenced)))
