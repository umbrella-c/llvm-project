#!/usr/bin/env python3
"""Build and inspect Vali ARM64 images and compiler-rt, without executing them.
The only imported runtime is an explicit test contract, not a host library.
Use --out with a fresh directory. No SDK installation is changed.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin', required=True, type=Path)
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[3])
p.add_argument('--out', required=True, type=Path)
a = p.parse_args()
a.bin = a.bin.resolve()
a.source = a.source.resolve()
a.out = a.out.resolve()
a.out.mkdir(parents=True, exist_ok=False)
fixtures = a.source / 'clang/test/CodeGen/Vali/Inputs/arm64'
commands = []


def run(name, args):
    args = list(map(str, args))
    cp = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (a.out / (name + '.log')).write_text('$ ' + ' '.join(args) + '\n' + cp.stdout)
    commands.append({'name': name, 'returncode': cp.returncode})
    if cp.returncode:
        sys.exit(f'{name} failed: see {a.out / (name + ".log")}')
    return cp.stdout


cc = [a.bin / 'clang', '--target=aarch64-uml-vali', '-ffreestanding',
      '-nostdlibinc', '-fms-extensions', '-funwind-tables', '-O1']
run('version', [a.bin / 'clang', '--version'])
run('model', cc + ['-DVALI_ABI_CONTRACT', '-fsyntax-only', fixtures / 'model.c'])
run('abi', cc + ['-fsyntax-only', a.source / 'clang/test/CodeGen/Vali/aarch64-abi.c'])
run('macros', cc + ['-dM', '-E', '-x', 'c', '/dev/null'])
for name in ['features', 'library', 'image', 'kernel', 'assembler', 'tls-directory']:
    source = fixtures / (name + ('.S' if name == 'assembler' else '.c'))
    obj = a.out / (name + '.obj')
    extra = ['-mgeneral-regs-only'] if name == 'kernel' else []
    run(name, cc + extra + ['-c', source, '-o', obj])
    info = run(name + '-inspect', [a.bin / 'llvm-readobj', '--file-headers',
               '--sections', '--relocations', '--unwind', obj])
    assert 'COFF-ARM64' in info and 'IMAGE_FILE_MACHINE_ARM64' in info
    asm = run(name + '-disasm', [a.bin / 'llvm-objdump', '-dr', obj])
    if name in ('features', 'kernel'):
        assert not re.search(r'\b(?:cas[a-z]*|swp[a-z]*|ldadd[a-z]*)\s', asm)
        assert '__aarch64_' not in asm and 'ldaxr' in asm and 'stlxr' in asm
    if name == 'kernel':
        assert not re.search(r'\b(?:[qvdshb][0-9]+)\b', asm)
    if name == 'features':
        assert '__vali_tls_get_block' in asm and '__chkstk' in asm
        assert '.tls$' in info and '.CRT$XCU' in info and 'RuntimeFunction' in info
        assert 'IMAGE_REL_ARM64_SECREL_' in info
# The O0 pipeline and assembly roundtrip must work as well.
run('features-o0', cc + ['-O0', '-c', fixtures / 'features.c', '-o', a.out / 'features-o0.obj'])
run('features-asm', cc + ['-S', fixtures / 'features.c', '-o', a.out / 'features.s'])
run('features-roundtrip', cc + ['-c', a.out / 'features.s', '-o', a.out / 'features-roundtrip.obj'])

build = a.out / 'builtins'
args = ['cmake', '-G', 'Ninja', '-S', a.source / 'compiler-rt/lib/builtins', '-B', build,
        '-DCMAKE_SYSTEM_NAME=Linux', '-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON',
        '-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY',
        '-DCMAKE_AR=' + str(a.bin / 'llvm-ar'),
        '-DCMAKE_RANLIB=' + str(a.bin / 'llvm-ranlib'),
        '-DLLVM_CONFIG_PATH=' + str(a.bin / 'llvm-config'),
        '-DCMAKE_C_FLAGS=-ffreestanding -nostdlibinc -march=armv8-a -mno-outline-atomics']
for lang, compiler in [('C', 'clang'), ('CXX', 'clang++'), ('ASM', 'clang')]:
    args += [f'-DCMAKE_{lang}_COMPILER={a.bin / compiler}',
             f'-DCMAKE_{lang}_COMPILER_TARGET=aarch64-uml-vali']
run('builtins-configure', args)
run('builtins-build', ['cmake', '--build', build, '--parallel', '4'])
archives = list(build.rglob('libclang_rt.builtins-aarch64.a'))
assert len(archives) == 1, archives
archive = archives[0]
info = run('builtins-inspect', [a.bin / 'llvm-readobj', '--file-headers', archive])
assert 'format: elf' not in info.lower()
assert info.count('Format: COFF-ARM64') == info.count('Format: ')
members = run('builtins-members', [a.bin / 'llvm-ar', 't', archive])
assert 'chkstk.S' in members and 'divti3.c' in members
assert 'outline_atomic' not in members and 'cpu_model' not in members and 'sme' not in members
symbols = run('builtins-symbols', [a.bin / 'llvm-nm', archive])
defined, undefined = set(), set()
for line in symbols.splitlines():
    fields = line.split()
    if len(fields) == 2 and fields[0] == 'U':
        undefined.add(fields[1])
    elif len(fields) == 3 and fields[1].isupper():
        defined.add(fields[2])
# The C personality intentionally depends on libunwind's public ABI. These
# are runtime library dependencies, not missing arithmetic/atomic helpers.
unwind_api = {'_Unwind_GetIP', '_Unwind_GetLanguageSpecificData',
              '_Unwind_GetRegionStart', '_Unwind_SetGR', '_Unwind_SetIP'}
assert undefined - defined == unwind_api, sorted(undefined - defined)
assert {'__chkstk', '__divti3', '__udivmodti4', '__gcc_personality_v0'} <= defined
builtins_asm = run('builtins-disasm', [a.bin / 'llvm-objdump', '-d', archive])
assert not re.search(r'\b(?:cas[a-z]*|swp[a-z]*|(?:ld|st)(?:add|clr|eor|set|smax|smin|umax|umin)[a-z]*)\s', builtins_asm)

# Import declarations document unresolved OS obligations without inventing
# dummy definitions. The runtime DLL itself is deliberately not fabricated.
runtime = a.out / 'runtime.lib'
run('runtime-imports', [a.bin / 'llvm-dlltool', '-m', 'arm64', '-d', fixtures / 'runtime.def', '-l', runtime])
dll, lib, exe = (a.out / name for name in ['fixture.dll', 'fixture.dll.lib', 'fixture.exe'])
link = [a.bin / 'clang', '--target=aarch64-uml-vali', '-nostdlib',
        '--ld-path=' + str(a.bin / 'lld-link'), '-Wl,/nodefaultlib,/manifest:no,/dynamicbase,/opt:noref']
run('dll-link', link + ['-shared', '-Wl,/noentry,/include:_tls_used',
    '-o', dll, a.out / 'library.obj', a.out / 'features.obj', a.out / 'tls-directory.obj', archive, runtime])
run('exe-link', link + ['-Wl,/entry:entry,/subsystem:console', '-o', exe, a.out / 'image.obj', lib])
for image in (dll, exe):
    info = run(image.name + '-inspect', [a.bin / 'llvm-readobj', '--file-headers',
        '--coff-imports', '--coff-exports', '--coff-basereloc', '--coff-tls-directory', '--unwind', image])
    assert 'IMAGE_FILE_MACHINE_ARM64' in info and 'Magic: 0x20B' in info
    imported_dlls = re.findall(r'^  Name: (.*\.dll)$', info, re.M)
    assert imported_dlls == (['vali-abi-test-runtime.dll'] if image == dll else ['fixture.dll']), imported_dlls
    if image == dll:
        assert '__vali_tls_get_block' in info and 'escape' in info
        assert 'TLSDirectory {' in info and 'RuntimeFunction {' in info
run('rebase', [sys.executable, fixtures / 'rebase.py', dll])
(a.out / 'summary.json').write_text(json.dumps({'compiler_link_gate': 'PASS',
    'runtime_execution': 'NOT RUN; real Vali ARM64 CRT/loader required',
    'commands': commands}, indent=2) + '\n')
print(f'PASS: compiler, builtins, DLL/executable, imports, TLS metadata, unwind and offline rebase.\nEvidence: {a.out}\nRuntime execution: NOT RUN.')
