# REQUIRES: x86-registered-target
# RUN: not llvm-mc -triple i386-uml-vali -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple x86_64-uml-vali -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
.text
entry:
.safeseh entry
.seh_proc entry
.seh_endproc
# CHECK: error: unknown directive
# CHECK-NEXT: .safeseh entry
# CHECK: error: unknown directive
# CHECK-NEXT: .seh_proc entry
# CHECK: error: unknown directive
# CHECK-NEXT: .seh_endproc
