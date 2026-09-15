"""Check the PE contracts consumed by Vali's processd and libos.

Parse bytes directly so a matching bug in LLVM's writer and reader cannot hide
a loader incompatibility. This does not execute the image or emulate the OS.
"""

import struct
import sys
from pathlib import Path


class Image:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        assert self.data[:2] == b"MZ", "Vali requires an MZ header"
        pe = self.u32(0x3C)
        assert self.data[pe : pe + 4] == b"PE\0\0", "Vali requires PE magic"
        self.machine = self.u16(pe + 4)
        count = self.u16(pe + 6)
        self.characteristics = self.u16(pe + 22)
        opt = pe + 24
        magic = self.u16(opt)
        assert magic in (0x10B, 0x20B), "expected PE32 or PE32+"
        self.pointer_size = 8 if magic == 0x20B else 4
        self.image_base = self.uint(opt + (24 if magic == 0x20B else 28),
                                    self.pointer_size)
        self.entry = self.u32(opt + 16)
        self.subsystem = self.u16(opt + 68)
        num_dirs = self.u32(opt + (108 if magic == 0x20B else 92))
        assert num_dirs >= 13
        dirs = opt + (112 if magic == 0x20B else 96)
        self.directories = [struct.unpack_from("<II", self.data, dirs + 8 * i)
                            for i in range(num_dirs)]
        section_table = opt + self.u16(pe + 20)
        self.sections = {}
        for i in range(count):
            off = section_table + i * 40
            name = self.data[off : off + 8].rstrip(b"\0")
            virtual_size, rva, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, off + 8)
            self.sections[name] = (rva, virtual_size, raw_offset, raw_size)

    def uint(self, offset, size):
        return struct.unpack_from({2: "<H", 4: "<I", 8: "<Q"}[size],
                                  self.data, offset)[0]

    def u16(self, offset):
        return self.uint(offset, 2)

    def u32(self, offset):
        return self.uint(offset, 4)

    def file_offset(self, rva, size):
        for start, virtual_size, raw_offset, raw_size in self.sections.values():
            delta = rva - start
            if 0 <= delta and delta + size <= min(virtual_size, raw_size):
                assert raw_offset + delta + size <= len(self.data)
                return raw_offset + delta
        raise AssertionError(f"RVA {rva:#x} (size {size}) is not file-backed")


mode, path, *args = sys.argv[1:]
image = Image(path)
if mode == "headers":
    machine, entry, kind = args
    assert image.machine == int(machine, 0)
    assert image.entry == int(entry, 0), hex(image.entry)
    assert image.subsystem == 1, "expected IMAGE_SUBSYSTEM_NATIVE"
    assert bool(image.characteristics & 0x2000) == (kind == "dll")
    assert image.directories[8][1] == 0, "unexpected pseudo relocations"
elif mode == "pseudo":
    assert image.subsystem == 1
    rva, size = image.directories[8]
    assert rva != 0, "loader cannot discover the pseudo-relocation list"
    assert size == 12 + 2 * 12, (rva, size)
    off = image.file_offset(rva, size)
    assert struct.unpack_from("<III", image.data, off) == (0, 0, 1)
    iat_rva, iat_size = image.directories[12]
    assert iat_rva and iat_size
    widths = []
    for i in range(2):
        symbol, target, flags = struct.unpack_from("<III", image.data,
                                                  off + 12 + i * 12)
        assert iat_rva <= symbol < iat_rva + iat_size
        image.file_offset(symbol, image.pointer_size)
        widths.append(flags & 0xFF)
        image.file_offset(target, (flags & 0xFF) // 8)
    assert sorted(widths) == sorted([32, image.pointer_size * 8]), widths
    # The fixture stores the list boundary addresses immediately after its
    # imported-data pointer. Compare them to the actual loader directory.
    data_rva = image.sections[b".data"][0]
    data = image.file_offset(data_rva, 3 * image.pointer_size)
    begin = image.uint(data + image.pointer_size, image.pointer_size)
    end = image.uint(data + 2 * image.pointer_size, image.pointer_size)
    assert begin == image.image_base + rva
    assert end - begin == size
elif mode == "no-directory":
    assert image.directories[8] == (0, 0), image.directories[8]
elif mode == "unwind":
    text_rva, text_size, _, _ = image.sections[b".text"]
    assert text_rva <= image.entry < text_rva + text_size
    # PE section names have eight inline bytes; libos compares that prefix.
    rva, size, _, _ = image.sections[b".eh_fram"]
    assert size > 0
    image.file_offset(rva, size)
else:
    raise AssertionError(f"unknown check: {mode}")
