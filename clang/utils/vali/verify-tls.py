#!/usr/bin/env python3
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Test real Vali ARM64 TLS access/switching in linked PE images under qemu-user.

This supplies eager module blocks and worker anchors; it does not qualify the
Vali loader, scheduler, CRT lifecycle or kernel context switching.
"""
import argparse
import json
import resource
from pathlib import Path
import struct
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'libunwind/utils'))
from vali_pe import PE

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin', type=Path, required=True)
p.add_argument('--vali', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--lifecycle', action='store_true')
p.add_argument('--qemu', default='qemu-aarch64')
a = p.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
a.bin, a.vali, out = a.bin.resolve(), a.vali.resolve(), a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
inputs = root / 'clang/test/CodeGen/Vali/Inputs/tls'


def run(name, cmd, trap=False):
    result = subprocess.run(list(map(str, cmd)), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=60)
    (out / (name + '.log')).write_text(' '.join(map(str, cmd)) + '\n' + result.stdout)
    # AArch64 __builtin_trap lowers to BRK, delivered as SIGTRAP by qemu-user.
    expected = -5 if trap else 0
    if result.returncode != expected:
        raise RuntimeError(f'{name}: exit {result.returncode}, expected {expected}\n{result.stdout}')
    return result.stdout


resource = run('resource', [a.bin / 'clang', '-print-resource-dir']).strip()
flags = ['--target=aarch64-uml-vali', '-O2', '-ffreestanding', '-nostdinc',
         '-fms-extensions', '-ffunction-sections', '-fdata-sections',
         '-I' + resource + '/include']
flags += ['-I' + str(a.vali / f'librt/{lib}/include') for lib in ['libc', 'libos', 'libddk', 'libds']]


def compile(source, name, extra=()):
    obj = out / (name + '.obj')
    run(name, [a.bin / 'clang', *flags, *extra, '-c', source, '-o', obj])
    return obj


locals_obj = compile(inputs / 'locals.c', 'locals')
tls = compile(a.vali / 'librt/libc/os/tls.c', 'os-tls')
template = compile(inputs / 'template.S', 'template')
unavailable = compile(inputs / 'unavailable.S', 'unavailable')
kernel = compile(inputs / 'kernel.c', 'kernel', ['-DLIBC_KERNEL', '-mgeneral-regs-only'])
assembly = run('kernel-inspect', [a.bin / 'llvm-objdump', '-d', kernel]).lower()
assert 'tpidr_el1' in assembly and 'tpidr_el0' not in assembly
assembly = run('user-inspect', [a.bin / 'llvm-objdump', '-d', tls]).lower()
assert 'tpidr_el0' in assembly and 'tpidr_el1' not in assembly
# Existing x86-64 slot assembly must remain compilable with the shared headers.
compile(inputs / 'kernel.c', 'x64-control', ['--target=x86_64-uml-vali'])
exports = out / 'runtime.def'
runtime_exports = ['__vali_tls_get_block']
if a.lifecycle:
    runtime_exports += ['__tls_register_module', '__tls_prepare_modules', '__cxa_callinitializers',
                        '__cxa_callinitializers_ex', '__cxa_callinitializers_tls',
                        '__cxa_tls_thread_cleanup', '__cxa_tls_module_cleanup']
exports.write_text('LIBRARY tls.exe\nEXPORTS\n' + '\n'.join(runtime_exports) + '\n')
run('import-lib', [a.bin / 'llvm-dlltool', '-m', 'arm64', '-d', exports,
                   '-l', out / 'runtime.lib'])
link = [a.bin / 'lld', '-flavor', 'link', '-lldvpe', '/machine:arm64',
        '/nodefaultlib', '/subsystem:console', '/opt:ref']
dll = out / 'tls-test.dll'
if a.lifecycle:
    crt = compile(a.vali / 'librt/libcrt/crt/crtcoff.c', 'crtcoff')
    # All three COFF targets add section-relative variable offsets. The TLS
    # directory's template pointer must therefore have a zero sentinel addend.
    for arch, obj, width in [('aarch64', crt, 8),
                             ('x86_64', compile(a.vali / 'librt/libcrt/crt/crtcoff.c',
                                                'crtcoff-x64', ['--target=x86_64-uml-vali']), 8),
                             ('i386', compile(a.vali / 'librt/libcrt/crt/crtcoff.c',
                                              'crtcoff-x86', ['--target=i686-uml-vali']), 4)]:
        data = obj.read_bytes()
        sections = struct.unpack_from('<H', data, 2)[0]
        symbols, symbol_count = struct.unpack_from('<II', data, 8)
        found = False
        for n in range(sections):
            off = 20 + n * 40
            if data[off:off + 8] != b'.rdata$T':
                continue
            raw, reloc = struct.unpack_from('<II', data, off + 20)
            assert int.from_bytes(data[raw:raw + width], 'little') == 0, arch
            address, symbol = struct.unpack_from('<II', data, reloc)
            assert address == 0
            sym = symbols + symbol * 18
            name = data[sym:sym + 8]
            if name[:4] == bytes(4):
                start = symbols + symbol_count * 18 + struct.unpack_from('<I', name, 4)[0]
                name = data[start:data.index(0, start)]
            assert name.rstrip(b'\0').lstrip(b'_') == b'tls_start', arch
            found = True
        assert found, arch
    library = compile(a.vali / 'librt/libcrt/library.c', 'library')
    template = crt
    unavailable = compile(inputs / 'lifecycle-unavailable.S', 'unavailable')
    extra_objects = [compile(root / 'compiler-rt/lib/builtins/aarch64/chkstk.S', 'chkstk'),
                     compile(a.vali / 'librt/libc/threads/tss.c', 'tss'),
                     compile(a.vali / 'librt/libds/hashtable.c', 'hashtable'),
                     compile(a.vali / 'librt/libc/os/tls_modules.c', 'tls_modules'),
                     compile(a.vali / 'librt/libc/os/clang.c', 'crt-dispatch'),
                     compile(a.vali / 'librt/libos/spinlock.c', 'spinlock'),
                     compile(inputs / 'lifecycle-platform.c', 'platform')]
    dll_objects = [crt, library, compile(inputs / 'lifecycle-dll.c', 'dll')]
else:
    extra_objects = []
    dll_objects = [template, compile(inputs / 'dll.c', 'dll')]
run('dll-link', [*link, '/dll', '/noentry', '/out:' + str(dll),
                 '/implib:' + str(out / 'dll.lib'), *dll_objects,
                 *(['/export:__CrtLibraryEntry'] if a.lifecycle else []), out / 'runtime.lib'])
results = []
for case in (['normal', 'CAPACITY'] if a.lifecycle else ['normal', 'INVALID_INDEX', 'MISSING_BLOCK', 'MISSING_CONTEXT']):
    obj = compile(inputs / ('lifecycle.c' if a.lifecycle else 'runtime.c'), case, [] if case == 'normal' else ['-D' + case])
    image = out / 'tls.exe'
    run(case + '-link', [*link, '/entry:entry', '/out:' + str(image),
                         *['/export:' + e for e in runtime_exports], '/include:initialized', *extra_objects, tls, template, unavailable, locals_obj, obj, out / 'dll.lib'])
    inspect = run(case + '-inspect', [a.bin / 'llvm-readobj', '--coff-imports', image])
    assert 'Name: tls-test.dll' in inspect and inspect.count('Name: ') == 1
    for base in [0x180000000, 0x190000000, 0x170000000]:
        images = [PE(image, base), PE(dll, base + 0x40000000)]
        exports = {pe.name: pe.exports() for pe in images}
        for pe in images:
            pe.resolve(exports)
        ident = b'\x7fELF\x02\x01\x01' + bytes(9)
        hdr = ident + struct.pack('<HHIQQQIHHHHHH', 2, 183, 1, images[0].entry,
                                  64, 0, 0, 64, 56, 2, 0, 0, 0)
        offset, payload, phdr = 4096, bytearray(), bytearray()
        for pe in images:
            size = len(pe.mapped)
            phdr += struct.pack('<IIQQQQQQ', 1, 7, offset, pe.base, pe.base, size, size, 4096)
            payload += pe.mapped
            offset += size
        elf = out / f'{case}-{base:x}.elf'
        elf.write_bytes((hdr + phdr).ljust(4096, b'\0') + payload)
        elf.chmod(0o755)
        run(elf.stem, [a.qemu, '-cpu', 'cortex-a53', elf], trap=case in ['INVALID_INDEX', 'MISSING_BLOCK', 'MISSING_CONTEXT'])
        results.append({'case': case, 'base': hex(base), 'result': 'PASS'})
(out / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
