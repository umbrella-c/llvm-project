#!/usr/bin/env python3
"""Offline PE mapping/rebase check, NOT execution or a Vali loader test."""
import struct
import sys
from pathlib import Path

raw = Path(sys.argv[1]).read_bytes()
u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
u64 = lambda b, o: struct.unpack_from('<Q', b, o)[0]
pe = u32(raw, 0x3c)
assert raw[pe:pe+4] == b'PE\0\0'
assert u16(raw, pe+4) == 0xaa64, 'not ARM64'
opt = pe + 24
assert u16(raw, opt) == 0x20b, 'not PE32+'
base = u64(raw, opt+24)
mapped = bytearray(u32(raw, opt+56))
headers = u32(raw, opt+60)
mapped[:headers] = raw[:headers]
sections = opt + u16(raw, pe+20)
for i in range(u16(raw, pe+6)):
    s = sections + 40*i
    rva, size, offset = (u32(raw, s+x) for x in (12, 16, 20))
    mapped[rva:rva+size] = raw[offset:offset+size]
rva, size = struct.unpack_from('<II', raw, opt+112+5*8)
assert rva and size, 'missing image base relocations'
original = bytes(mapped)
end = rva + size
sites = []
while rva < end:
    page, block = struct.unpack_from('<II', mapped, rva)
    assert block >= 8 and block % 2 == 0 and rva+block <= end
    for o in range(rva+8, rva+block, 2):
        entry = u16(mapped, o)
        kind, offset = entry >> 12, entry & 0xfff
        if kind == 0:
            continue
        assert kind == 10, f'unexpected base relocation type {kind}'
        site = page + offset
        assert site+8 <= len(mapped)
        sites.append(site)
    rva += block
assert sites and len(sites) == len(set(sites))
# Locate the exported pointer through the export address table.
exp = u32(raw, opt+112)
assert exp
functions, names, ordinals = (u32(mapped, exp+x) for x in (28, 32, 36))
exports = {}
for i in range(u32(mapped, exp+24)):
    n = u32(mapped, names+4*i)
    name = mapped[n:mapped.index(0, n)].decode()
    exports[name] = u32(mapped, functions+4*u16(mapped, ordinals+2*i))
pointer = exports['exported_pointer']
assert pointer in sites
assert u64(original, pointer) == base + exports['exported_data']
for delta in (0x200000, -0x100000):
    relocated = bytearray(original)
    for site in sites:
        struct.pack_into('<Q', relocated, site, u64(original, site) + delta)
    assert u64(relocated, pointer) == base + delta + exports['exported_data']
print(f'ARM64 PE32+: {len(sites)} DIR64 sites; exported pointer rebased up/down (offline only)')
