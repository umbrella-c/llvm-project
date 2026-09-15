# REQUIRES: x86
# RUN: split-file %s %t.dir
# RUN: llvm-mc -triple=i686-windows-gnu -filetype=obj %t.dir/main.s -o %t.main.obj
# RUN: llvm-mc -triple=i686-windows-gnu -filetype=obj %t.dir/lib.s -o %t.lib.obj
# RUN: lld-link /lldvpe /out:%t.exe %t.main.obj %t.lib.obj 2>&1 | FileCheck %s --check-prefix=FIXUP
# RUN: not lld-link /lldvpe /stdcall-fixup:no /out:%t.exe %t.main.obj %t.lib.obj 2>&1 | FileCheck %s --check-prefix=DISABLED
# RUN: lld-link /lldvpe /stdcall-fixup /out:%t.exe %t.main.obj %t.lib.obj

# FIXUP: Resolving _answer@0 by linking to _answer
# DISABLED: error: {{(undefined symbol|relocation against symbol in discarded section)}}: _answer@0

#--- main.s
  .text
  .globl ___CrtConsoleEntry
___CrtConsoleEntry:
  call _answer@0
  ret

#--- lib.s
  .text
  .globl _answer
_answer:
  movl $42, %eax
  ret
