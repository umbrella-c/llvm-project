#!/usr/bin/env python3
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Execute Vali ARM64 PE exception code under qemu-user, with no host libraries.

The test platform supplies allocation, diagnostics and module lookup; exception
handling itself uses real compiler-rt, libunwind and libc++abi sources. An ELF
load envelope lets qemu-user map the already linked PE image. This is a runtime
ABI test, not a Vali loader, TLS or operating-system qualification.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
from vali_pe import PE

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin', type=Path, required=True)
p.add_argument('--vali', type=Path, required=True, help='Vali checkout (C/OS headers)')
p.add_argument('--out', type=Path, required=True)
p.add_argument('--threads', action='store_true', help='Test logical-job TLS with thread-enabled libc++abi')
p.add_argument('--qemu', default='qemu-aarch64')
p.add_argument('--builtins', type=Path, help='Validate against the complete Vali compiler-rt archive')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
a.bin = a.bin.resolve()
a.vali = a.vali.resolve()
inputs = root / 'libunwind/test/Inputs/Vali'


def run(name, cmd):
    cmd = list(map(str, cmd))
    result = subprocess.run(cmd, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=180)
    (out / (name + '.log')).write_text(' '.join(cmd) + '\n' + result.stdout)
    if result.returncode:
        raise RuntimeError(f'{name} failed ({result.returncode}):\n{result.stdout}')
    return result.stdout


run('decoder', ['c++', '-std=c++11', '-Wall', '-Wextra',
                root / 'libunwind/test/pe-arm64-decoder.pass.cpp', '-o', out / 'decoder'])
run('decoder-run', [out / 'decoder'])
inc = out / 'include'
inc.mkdir(exist_ok=True)
config = (root / 'libcxx/include/__config_site.in').read_text()
config = re.sub(r'^#cmakedefine01 (\w+)$', r'#define \1 0', config, flags=re.M)
config = re.sub(r'^#cmakedefine .*$', '', config, flags=re.M)
config = re.sub(r'@\w+@', '', config)
config = config.replace('#define _LIBCPP_ABI_FORCE_ITANIUM 0',
                        '#define _LIBCPP_ABI_FORCE_ITANIUM 1')
config += '''
#define _LIBCPP_ABI_VERSION 1
#define _LIBCPP_ABI_NAMESPACE __1
#define _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS
#define _LIBCPP_HARDENING_MODE_DEFAULT _LIBCPP_HARDENING_MODE_NONE
#define _LIBCPP_ASSERTION_SEMANTIC_DEFAULT _LIBCPP_ASSERTION_SEMANTIC_IGNORE
'''
if a.threads:
    config = config.replace('#define _LIBCPP_HAS_MONOTONIC_CLOCK 0', '#define _LIBCPP_HAS_MONOTONIC_CLOCK 1')
    config = config.replace('#define _LIBCPP_HAS_THREADS 0', '#define _LIBCPP_HAS_THREADS 1')
    config = config.replace('#define _LIBCPP_HAS_THREAD_API_C11 0', '#define _LIBCPP_HAS_THREAD_API_C11 1')
(inc / '__config_site').write_text(config)
shutil.copyfile(root / 'libcxx/vendor/llvm/default_assertion_handler.in', inc / '__assertion_handler')
resource = run('resource', [a.bin / 'clang', '-print-resource-dir']).strip()
common = [a.bin / 'clang', '--target=aarch64-uml-vali', '-O1', '-g',
          '-funwind-tables', '-ffreestanding', '-nostdinc', '-fms-extensions',
          '-D__BITS=64', '-D__STDC_FORMAT_MACROS', '-DVALI',
          '-D_LIBUNWIND_IS_NATIVE_ONLY', '-D_LIBUNWIND_HIDE_SYMBOLS',
          '-I' + str(root / 'libunwind/include'),
          '-I' + str(a.vali / 'librt/libc/include'),
          '-I' + str(a.vali / 'librt/libos/include'),
          '-I' + resource + '/include']
objects = []


def compile(source, extra=()):
    source = Path(source)
    obj = out / (source.stem + '.obj')
    run(source.stem, common[:1] + list(extra) + common[1:] + ['-c', source, '-o', obj])
    objects.append(obj)


for name in ['libunwind.cpp', 'UnwindLevel1.c', 'UnwindLevel1-gcc-ext.c',
             'UnwindRegistersSave.S', 'UnwindRegistersRestore.S']:
    compile(root / 'libunwind/src' / name,
            ['-fno-exceptions', '-fno-rtti'] if name.endswith('.cpp') else ['-fexceptions'])
unwind_archive = out / 'libunwind-arm64.a'
run('unwind-archive', [a.bin / 'llvm-ar', 'rcs', unwind_archive] + objects)
objects = [unwind_archive]

cxxflags = ['-std=c++23', '-fexceptions', '-fcxx-exceptions',
            '-D_LIBCPP_BUILDING_LIBRARY', '-D_LIBCXXABI_BUILDING_LIBRARY',
            '-D_LIBCXXABI_HAS_NO_THREADS', '-D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS',
            '-I' + str(inc), '-I' + str(root / 'libcxx/include'),
            '-I' + str(root / 'libcxxabi/include'), '-I' + str(root / 'libcxxabi/src'), '-I' + str(root / 'libcxx/src')]
if a.threads:
    cxxflags.remove('-D_LIBCXXABI_HAS_NO_THREADS')
    cxxflags += ['-DHAVE___CXA_THREAD_ATEXIT_IMPL', '-DTEST_THREADED_TLS']
    common += ['-I' + str(a.vali / 'librt/libddk/include'),
               '-I' + str(a.vali / 'librt/libds/include')]
for name in ['cxa_personality', 'cxa_exception', 'cxa_exception_storage',
             'cxa_handlers', 'cxa_aux_runtime', 'private_typeinfo',
             'stdlib_typeinfo', 'stdlib_exception', 'fallback_malloc', 'cxa_virtual']:
    compile(root / 'libcxxabi/src' / (name + '.cpp'), cxxflags)
if a.threads:
    compile(root / 'libcxxabi/src/cxa_thread_atexit.cpp', cxxflags)
    for path in ['librt/libc/os/tls.c', 'librt/libc/os/tls_modules.c',
                 'librt/libc/os/clang.c', 'librt/libos/spinlock.c',
                 'librt/libc/threads/tss.c', 'librt/libds/hashtable.c',
                 'librt/libcrt/crt/crtcoff.c']:
        compile(a.vali / path)
    compile(inputs / 'thread-platform.c')
    compile(a.vali / 'librt/libc/arch/aarch64/_setjmp.S')
    compile(a.vali / 'librt/libc/arch/aarch64/_fpreset.S')
    kernel_jmp = out / 'kernel-setjmp.obj'
    run('kernel-setjmp', common + ['-DLIBC_KERNEL', '-mgeneral-regs-only', '-c',
                                  a.vali / 'librt/libc/arch/aarch64/_setjmp.S', '-o', kernel_jmp])
    kernel_asm = run('kernel-setjmp-inspect', [a.bin / 'llvm-objdump', '-d', kernel_jmp]).lower()
    assert not re.search(r'\b(?:[dqvs][0-9]+|fpcr|fpsr)\b', kernel_asm)

    compile(a.vali / 'librt/libos/uthreads/aarch64/context.S')
    compile(inputs / 'thread-unavailable.S')
    compile(inputs / 'thread-exceptions.cpp', cxxflags)
if a.builtins:
    objects.append(a.builtins.resolve())
else:
    compile(root / 'compiler-rt/lib/builtins/gcc_personality_v0.c', ['-fexceptions'])
    compile(root / 'compiler-rt/lib/builtins/aarch64/chkstk.S')
    compile(root / 'compiler-rt/lib/builtins/int_util.c')
compile(inputs / 'platform.c')
compile(a.vali / 'librt/libos/unwind.c', ['-I' + str(inputs / 'include')])
compile(inputs / 'exceptions.cpp', cxxflags)
compile(inputs / 'cleanup.c', ['-fexceptions'])
compile(inputs / 'support.cpp', cxxflags)
# The DLL shares the executable's exception runtime, just as production
# modules must share their process runtime. Import libraries describe real
# definitions in these images, not dummy personality/unwinder stubs.
exports = ['__cxa_allocate_exception', '__cxa_throw', '__gxx_personality_v0',
           '_Unwind_Resume', '_ZTIi DATA', 'record_cleanup']
main_def = out / 'runtime.def'
main_def.write_text('LIBRARY exceptions.exe\nEXPORTS\n' + '\n'.join(exports) + '\n')
run('runtime-import-lib', [a.bin / 'llvm-dlltool', '-m', 'arm64', '-d', main_def,
                          '-l', out / 'runtime.lib'])
compile(inputs / 'dll.cpp', cxxflags)
dll_obj = objects.pop()
dll = out / 'exception-test.dll'
link = [a.bin / 'lld', '-flavor', 'link', '-lldvpe', '/machine:arm64', '/nodefaultlib',
        '/subsystem:console']
run('dll-link', link + ['/dll', '/noentry', '/base:0x1c0000000',
                      '/out:' + str(dll), '/implib:' + str(out / 'dll.lib'),
                      dll_obj, out / 'runtime.lib'])
image = out / 'exceptions.exe'
run('link', link + ['/entry:entry', '/base:0x180000000', '/out:' + str(image)] +
    ['/export:' + e.replace(' DATA', ',DATA') for e in exports] +
    objects + [out / 'dll.lib'])
for pe in [image, dll]:
    inspect = run(pe.stem + '-inspect', [a.bin / 'llvm-readobj', '--file-headers',
                   '--coff-imports', '--unwind', '--coff-basereloc', pe])
    assert 'IMAGE_FILE_MACHINE_ARM64' in inspect
    imports = re.findall(r'^  Name: (.*)$', inspect, re.M)
    assert imports == (['exception-test.dll'] if pe == image else ['exceptions.exe'])
    disasm = run(pe.stem + '-disasm', [a.bin / 'llvm-objdump', '-d', pe])
    assert not re.search(r'\b(?:cas[a-z]*|swp[a-z]*|(?:ld|st)(?:add|clr|eor|set|smax|smin|umax|umin)[a-z]*)\s', disasm)



def envelope(new_base):
    images = [PE(image, new_base), PE(dll, new_base + 0x40000000)]
    exports = {pe.name: pe.exports() for pe in images}
    for pe in images:
        pe.resolve(exports)
    assert images[0].relocations
    # PT_LOAD segments carry the linked PE mappings, with DIR64 and IAT fixups.
    ident = b'\x7fELF\x02\x01\x01' + bytes(9)
    hdr = ident + struct.pack('<HHIQQQIHHHHHH', 2, 183, 1, images[0].entry,
                              64, 0, 0, 64, 56, len(images), 0, 0, 0)
    offset, payload, phdr = 4096, bytearray(), bytearray()
    for pe in images:
        size = len(pe.mapped)
        phdr += struct.pack('<IIQQQQQQ', 1, 7, offset, pe.base, pe.base, size, size, 4096)
        payload += pe.mapped
        offset += size
    elf = out / f'exceptions-{new_base:x}.elf'
    elf.write_bytes((hdr + phdr).ljust(4096, b'\0') + payload)
    elf.chmod(0o755)
    result = run(elf.stem, [a.qemu, '-cpu', 'cortex-a53', elf])
    assert 'PASS Vali ARM64 exceptions' in result
    return {'base': hex(new_base), 'DIR64_relocations': sum(pe.relocations for pe in images),
            'result': result.strip()}

results = [envelope(base) for base in (0x180000000, 0x190000000, 0x170000000)]
(out / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
