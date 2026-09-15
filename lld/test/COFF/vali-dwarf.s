# REQUIRES: x86
# RUN: llvm-mc -triple=x86_64-windows-gnu -filetype=obj %s -o %t.obj
# RUN: lld-link /lldvpe /debug:dwarf /out:%t.exe %t.obj
# RUN: %python %S/Inputs/vali-verify-image.py unwind %t.exe
# RUN: llvm-dwarfdump --debug-line %t.exe | FileCheck %s
# RUN: llvm-dwarfdump --eh-frame %t.exe | FileCheck %s --check-prefix=CFI
# RUN: llvm-dwarfdump --verify %t.exe

## libos locates .text and the raw eight-byte prefix of .eh_frame in memory.
## Source lines and exception CFI must both survive linking.
# CHECK: file_names[{{ *}}1]:
# CHECK: name: "vali-dwarf.c"
# CFI: FDE cie=

  .file 1 "vali-dwarf.c"
  .text
  .globl __CrtConsoleEntry
__CrtConsoleEntry:
  .cfi_startproc
  .loc 1 1 0
  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp
  .loc 1 2 0
  popq %rbp
  .cfi_def_cfa %rsp, 8
  ret
  .cfi_endproc
