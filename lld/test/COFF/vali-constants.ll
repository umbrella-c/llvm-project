; REQUIRES: x86
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t64.obj
; RUN: lld-link /lldvpe /entry:value /out:%t64.run %t64.obj
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.obj
; RUN: lld-link /lldvpe /entry:value /out:%t32.run %t32.obj
; RUN: llc -mtriple=x86_64-pc-windows-msvc -filetype=obj %s -o %twin64.obj
; RUN: lld-link /entry:value /subsystem:console /out:%twin64.exe %twin64.obj
; RUN: llc -mtriple=i386-pc-windows-msvc -filetype=obj %s -o %twin32.obj
; RUN: lld-link /entry:value /subsystem:console /out:%twin32.exe %twin32.obj
;
; The constant pool must define its COFF COMDAT key instead of a local CPI
; label that leaves __real@4530000000100000 undefined at link time.
define double @value(double %x) {
  %r = fadd double %x, 0x4530000000100000
  ret double %r
}

; Windows CRT marker; Vali does not require this symbol.
@_fltused = global i32 0
