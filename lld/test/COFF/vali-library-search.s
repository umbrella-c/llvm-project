# REQUIRES: x86
# RUN: rm -rf %t.dir
# RUN: split-file %s %t.dir
# RUN: llvm-mc -triple=x86_64-windows-gnu -filetype=obj %t.dir/lib.s -o %t.lib.obj
# RUN: lld-link /lldvpe /dll /noentry /export:answer /out:%t.dir/answer.dll /implib:%t.dir/answer.dll.lib %t.lib.obj
# RUN: llvm-mc -triple=x86_64-windows-gnu -filetype=obj %t.dir/main.s -o %t.main.obj
## A bare library input and an embedded /defaultlib must find answer.dll.lib.
# RUN: lld-link /lldvpe /libpath:%t.dir /out:%t.exe %t.main.obj answer.lib
# RUN: llvm-readobj --coff-imports %t.exe | FileCheck %s
# RUN: llvm-mc -triple=x86_64-windows-gnu -defsym directive=1 -filetype=obj %t.dir/main.s -o %t.directive.obj
# RUN: lld-link /lldvpe /libpath:%t.dir /out:%t.exe %t.directive.obj
# RUN: llvm-readobj --coff-imports %t.exe | FileCheck %s
## /nodefaultlib still suppresses embedded directives.
# RUN: not lld-link /lldvpe /nodefaultlib /libpath:%t.dir /out:%t.exe %t.directive.obj 2>&1 | FileCheck %s --check-prefix=MISSING
## An existing answer.lib wins over the suffix fallback.
# RUN: lld-link /lldvpe /dll /noentry /export:answer /out:%t.dir/preferred.dll /implib:%t.dir/answer.lib %t.lib.obj
# RUN: lld-link /lldvpe /libpath:%t.dir /out:%t.exe %t.main.obj answer.lib
# RUN: llvm-readobj --coff-imports %t.exe | FileCheck %s --check-prefix=PREFERRED

# CHECK: Name: answer.dll
# CHECK: Symbol: answer
# MISSING: error: {{(undefined symbol|relocation against symbol in discarded section)}}: answer
# PREFERRED: Name: preferred.dll
# PREFERRED: Symbol: answer

#--- lib.s
  .text
  .globl answer
answer:
  movl $42, %eax
  ret

#--- main.s
  .text
  .globl __CrtConsoleEntry
__CrtConsoleEntry:
  call answer
  ret
.ifdef directive
  .section .drectve
  .ascii " /defaultlib:answer.lib"
.endif
