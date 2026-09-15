; REQUIRES: x86
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.obj
; RUN: lld-link /lldvpe /out:%t32.exe %t32.obj
; RUN: llvm-readobj --coff-tls-directory %t32.exe | FileCheck %s
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t64.obj
; RUN: lld-link /lldvpe /out:%t64.exe %t64.obj
; RUN: llvm-readobj --coff-tls-directory %t64.exe | FileCheck %s
; RUN: %python %S/Inputs/vali-verify-tls.py %t32.exe %t64.exe
; CHECK: TLSDirectory {
; CHECK: SizeOfZeroFill: 0x0
;
; Model the CRT directory with its template starting at the section base:
; TLS SECREL offsets include the initial sentinel and alignment padding.
@_tls_start = global i8 0, section ".tls"
@value = thread_local global i32 42, align 4
@_tls_end = global i8 0, section ".tls$ZZZ"
@_tls_index = global i32 3
@callbacks = constant [1 x ptr] zeroinitializer
@_tls_used = constant {ptr, ptr, ptr, ptr, i32, i32} {
  ptr @_tls_start, ptr @_tls_end, ptr @_tls_index, ptr @callbacks, i32 0, i32 0
}, section ".rdata$T"

define i32 @__CrtConsoleEntry() {
  %v = load i32, ptr @value, align 4
  ret i32 %v
}
