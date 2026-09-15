# REQUIRES: x86
# RUN: llvm-mc -triple=x86_64-uml-vali -defsym=X64=1 -filetype=obj -dwarf-version=4 %s -o %t64.obj
# RUN: lld-link /lldvpe /debug:dwarf /out:%t64.exe %t64.obj
# RUN: %python %S/Inputs/vali-verify-image.py headers %t64.exe 0x8664 0x1000 exe
# RUN: %python %S/Inputs/vali-verify-image.py unwind %t64.exe
# RUN: llvm-dwarfdump --verify %t64.exe
# RUN: llvm-dwarfdump --eh-frame --debug-line %t64.exe | FileCheck %s
# RUN: llvm-mc -triple=i386-uml-vali -filetype=obj -dwarf-version=4 %s -o %t32.obj
# RUN: lld-link /lldvpe /debug:dwarf /out:%t32.exe %t32.obj
# RUN: %python %S/Inputs/vali-verify-image.py headers %t32.exe 0x14c 0x1000 exe
# RUN: %python %S/Inputs/vali-verify-image.py unwind %t32.exe
# RUN: llvm-dwarfdump --verify %t32.exe
# RUN: llvm-dwarfdump --eh-frame --debug-line %t32.exe | FileCheck %s

# This uses the Vali MC target directly, then checks the existing OS loader's
# header and unwind-section contracts in the final image.
.file 1 "vali-mc-dwarf.c"
.text
.ifdef X64
.globl __CrtConsoleEntry
__CrtConsoleEntry:
.else
.globl ___CrtConsoleEntry
___CrtConsoleEntry:
.endif
.cfi_startproc
.loc 1 1 0
nop
.loc 1 2 0
ret
.cfi_endproc

# CHECK: FDE cie=
# CHECK: name: "vali-mc-dwarf.c"
