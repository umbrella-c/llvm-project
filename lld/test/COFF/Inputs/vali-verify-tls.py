"""Check the TLS template origin, payload and PE directory field widths."""
import struct
import sys
from pathlib import Path

for filename in sys.argv[1:]:
    data = Path(filename).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    is64 = struct.unpack_from('<H', data, opt)[0] == 0x20b
    image_base = struct.unpack_from('<Q' if is64 else '<I', data,
                                    opt + (24 if is64 else 28))[0]
    directory = opt + (112 if is64 else 96) + 9 * 8
    tls_rva, tls_size = struct.unpack_from('<II', data, directory)
    assert tls_size == (40 if is64 else 24)
    sections = []
    for index in range(count):
        pos = opt + optional_size + index * 40
        name = data[pos:pos + 8].rstrip(b'\0')
        _, rva, size, offset = struct.unpack_from('<IIII', data, pos + 8)
        sections.append((name, rva, size, offset))
    def raw(rva):
        for _, base, size, offset in sections:
            if base <= rva < base + size:
                return offset + rva - base
        raise AssertionError(f'RVA {rva:x} has no file contents')
    start, end, index, callbacks, zero, flags = struct.unpack_from(
        '<QQQQII' if is64 else '<IIIIII', data, raw(tls_rva))
    section = next(s for s in sections if s[0] == b'.tls')
    assert start - image_base == section[1], 'TLS copy must preserve SECREL origin'
    assert end > start and zero == 0
    template = data[raw(start - image_base):raw(end - image_base)]
    assert struct.unpack_from('<I', template, 4)[0] == 42
    assert struct.unpack_from('<I', data, raw(index - image_base))[0] == 3
    assert struct.unpack_from('<Q' if is64 else '<I', data,
                              raw(callbacks - image_base))[0] == 0
