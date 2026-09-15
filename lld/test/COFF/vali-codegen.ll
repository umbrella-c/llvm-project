; REQUIRES: x86
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t64.obj
; RUN: lld-link /lldvpe /debug:dwarf /out:%t64.exe %t64.obj
; RUN: %python %S/Inputs/vali-verify-image.py headers %t64.exe 0x8664 0x1000 exe
; RUN: %python %S/Inputs/vali-verify-image.py unwind %t64.exe
; RUN: llvm-dwarfdump --eh-frame %t64.exe | FileCheck %s
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.obj
; RUN: lld-link /lldvpe /debug:dwarf /out:%t32.exe %t32.obj
; RUN: %python %S/Inputs/vali-verify-image.py headers %t32.exe 0x14c 0x1000 exe
; RUN: %python %S/Inputs/vali-verify-image.py unwind %t32.exe
; RUN: llvm-dwarfdump --eh-frame %t32.exe | FileCheck %s
; CHECK: FDE cie=

; Check the IR-to-object-to-native-image path without requiring an SDK.
define i32 @__CrtConsoleEntry() uwtable {
  ret i32 0
}
