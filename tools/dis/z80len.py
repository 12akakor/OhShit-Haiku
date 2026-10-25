# Copyright 2026, Akakor. Distributed under the terms of the MIT License.
"""Z80 instruction length and control flow, for tracing code."""

# Main opcode lengths without prefix.
_LEN = [1] * 256
for op in (0x06, 0x0e, 0x16, 0x1e, 0x26, 0x2e, 0x36, 0x3e,       # ld r,n
           0xc6, 0xce, 0xd6, 0xde, 0xe6, 0xee, 0xf6, 0xfe,       # alu n
           0x10, 0x18, 0x20, 0x28, 0x30, 0x38,                   # djnz, jr
           0xd3, 0xdb, 0xcb):                                    # out, in, CB
    _LEN[op] = 2
for op in (0x01, 0x11, 0x21, 0x31, 0x22, 0x2a, 0x32, 0x3a,
           0xc3, 0xcd) + tuple(range(0xc2, 0x100, 8)) + tuple(range(0xc4, 0x100, 8)):
    _LEN[op] = 3

# ED opcodes with a 16 bit operand.
_ED_NN = {0x43, 0x4b, 0x53, 0x5b, 0x63, 0x6b, 0x73, 0x7b}

# Opcodes that use (HL) and get a displacement with DD/FD.
_USES_HL_MEM = {0x34, 0x35, 0x36, 0x46, 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x7e,
                0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77,
                0x86, 0x8e, 0x96, 0x9e, 0xa6, 0xae, 0xb6, 0xbe}


def length(mem, a):
    """Length of the instruction at mem[a] (mem: callable or bytes)."""
    rd = mem if callable(mem) else (lambda x: mem[x & 0xffff])
    op = rd(a)
    if op == 0xed:
        op2 = rd(a + 1)
        return 4 if op2 in _ED_NN else 2
    if op in (0xdd, 0xfd):
        op2 = rd(a + 1)
        if op2 == 0xcb:
            return 4
        if op2 in (0xdd, 0xfd, 0xed):
            return 1        # prefix without effect
        n = _LEN[op2] + 1
        if op2 in _USES_HL_MEM:
            n += 1
        return n
    return _LEN[op]


def flow(mem, a):
    """(length, targets, falls_through) of the instruction at a.

    targets: static jump or call destinations. falls_through: execution
    may continue at a + length (conditional branches, calls, ordinary
    instructions)."""
    rd = mem if callable(mem) else (lambda x: mem[x & 0xffff])
    n = length(mem, a)
    op = rd(a)
    w = lambda: rd(a + 1) | (rd(a + 2) << 8)
    rel = lambda: (a + 2 + ((rd(a + 1) ^ 0x80) - 0x80)) & 0xffff
    if op == 0xc3:
        return n, [w()], False
    if op == 0xcd or (op & 0xc7) == 0xc4:
        return n, [w()], True
    if (op & 0xc7) == 0xc2:
        return n, [w()], True
    if op == 0x18:
        return n, [rel()], False
    if op in (0x10, 0x20, 0x28, 0x30, 0x38):
        return n, [rel()], True
    if op in (0xc9, 0xe9):                      # ret, jp (hl)
        return n, [], False
    if op in (0xdd, 0xfd) and rd(a + 1) == 0xe9:  # jp (ix)
        return n, [], False
    if op == 0xed and rd(a + 1) in (0x45, 0x4d):  # retn, reti
        return n, [], False
    if (op & 0xc7) == 0xc7:                     # rst
        return n, [op & 0x38], True
    return n, [], True
