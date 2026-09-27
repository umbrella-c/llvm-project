// REQUIRES: aarch64-registered-target
// RUN: llvm-mc -triple=aarch64-uml-vali -filetype=obj %s -o %t.obj
// RUN: llvm-readobj --file-headers --relocations --unwind %t.obj | FileCheck %s
// CHECK: Format: COFF-ARM64
// CHECK: Machine: IMAGE_FILE_MACHINE_ARM64
// CHECK: IMAGE_REL_ARM64_PAGEBASE_REL21 value
// CHECK: IMAGE_REL_ARM64_PAGEOFFSET_12A value
// CHECK: IMAGE_REL_ARM64_BRANCH26 external
// CHECK: IMAGE_REL_ARM64_ADDR64 value
// CHECK: RuntimeFunction {
.text
.globl entry
.seh_proc entry
entry:
  stp x29, x30, [sp, #-16]!
  .seh_save_fplr_x 16
  .seh_endprologue
  adrp x0, value
  add x0, x0, :lo12:value
  bl external
  .seh_startepilogue
  ldp x29, x30, [sp], #16
  .seh_save_fplr_x 16
  .seh_endepilogue
  ret
.seh_endproc
.data
value:
  .xword value
