# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Minimal PE mapper for ARM64 runtime fixtures, not a production loader."""
import struct


class PE:
    def __init__(self, path, new_base):
        data = path.read_bytes()
        u16 = lambda off: struct.unpack_from('<H', data, off)[0]
        u32 = lambda off: struct.unpack_from('<I', data, off)[0]
        self.base = new_base
        self.name = path.name
        nt = u32(0x3c)
        opt = nt + 24
        old_base = struct.unpack_from('<Q', data, opt + 24)[0]
        self.entry = new_base + u32(opt + 16)
        self.directories = [struct.unpack_from('<II', data, opt + 112 + i * 8)
                            for i in range(16)]
        self.mapped = bytearray(u32(opt + 56))
        headers = u32(opt + 60)
        self.mapped[:headers] = data[:headers]
        sections = nt + 24 + u16(nt + 20)
        for n in range(u16(nt + 6)):
            s = sections + 40 * n
            va, length, raw = u32(s + 12), u32(s + 16), u32(s + 20)
            self.mapped[va:va + length] = data[raw:raw + length]
        reloc, length = self.directories[5]
        end = reloc + length
        self.relocations = 0
        while reloc < end:
            page, block = struct.unpack_from('<II', self.mapped, reloc)
            assert block >= 8 and reloc + block <= end
            for r in range(reloc + 8, reloc + block, 2):
                entry = self.get('<H', r)
                kind, off = entry >> 12, page + (entry & 4095)
                if kind == 0:
                    continue
                assert kind == 10
                value = self.get('<Q', off)
                struct.pack_into('<Q', self.mapped, off, value + new_base - old_base)
                self.relocations += 1
            reloc += block

    def get(self, fmt, off):
        return struct.unpack_from(fmt, self.mapped, off)[0]

    def string(self, off):
        return self.mapped[off:self.mapped.index(0, off)].decode()

    def exports(self):
        d, _ = self.directories[0]
        count = self.get('<I', d + 24)
        eat, names, ordinals = struct.unpack_from('<III', self.mapped, d + 28)
        return {self.string(self.get('<I', names + n * 4)):
                self.base + self.get('<I', eat + self.get('<H', ordinals + n * 2) * 4)
                for n in range(count)}

    def resolve(self, exports):
        d, _ = self.directories[1]
        while d and self.get('<I', d + 12):
            ilt = self.get('<I', d)
            iat = self.get('<I', d + 16)
            library = exports[self.string(self.get('<I', d + 12))]
            while self.get('<Q', ilt):
                name = self.string(self.get('<Q', ilt) + 2)
                struct.pack_into('<Q', self.mapped, iat, library[name])
                ilt += 8
                iat += 8
            d += 20


