; RUN: llc -mtriple=i386-uml-vali %s -o - | FileCheck %s --check-prefix=X32
; RUN: llc -mtriple=x86_64-uml-vali %s -o - | FileCheck %s --check-prefix=X64
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.obj
; RUN: llvm-readobj --relocations %t32.obj | FileCheck %s --check-prefix=OBJ32
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t64.obj
; RUN: llvm-readobj --relocations %t64.obj | FileCheck %s --check-prefix=OBJ64
;
; Vali's i386 chkstk2.S adjusts ESP; x86_64 chkstk3.S preserves RSP.
; The libcrt integer helpers return with `ret 16` and have no @16 suffix.
; OBJ32: IMAGE_REL_I386_REL32 __chkstk
; OBJ32: IMAGE_REL_I386_REL32 __alldiv
; OBJ32: IMAGE_REL_I386_REL32 __aulldiv
; OBJ32: IMAGE_REL_I386_REL32 __allrem
; OBJ32: IMAGE_REL_I386_REL32 __aullrem
; OBJ64: IMAGE_REL_AMD64_REL32 __chkstk
;
declare void @consume(ptr)
; X32-LABEL: _large:
; X32: calll __chkstk
; X32-NOT: subl %eax, %esp
; X32: calll _consume
; X32: retl
; X64-LABEL: large:
; X64: callq __chkstk
; X64-NEXT: subq %rax, %rsp
; X64: callq consume
; X64: retq
define void @large() {
  %p = alloca [8192 x i8], align 16
  call void @consume(ptr %p)
  ret void
}
; X32-LABEL: _dynamic:
; X32: calll __chkstk
; X32-NOT: subl %eax, %esp
; X32: calll _consume
; X32: retl
; X64-LABEL: dynamic:
; X64: callq __chkstk
; X64-NEXT: subq %rax, %rsp
; X64: callq consume
; X64: retq
define void @dynamic(i32 %n) {
  %p = alloca i8, i32 %n, align 16
  call void @consume(ptr %p)
  ret void
}
; X32-LABEL: _suppressed:
; X32-NOT: chkstk
; X32: subl ${{[0-9]+}}, %esp
; X32-NOT: chkstk
; X32: retl
; X64-LABEL: suppressed:
; X64-NOT: chkstk
; X64: subq ${{[0-9]+}}, %rsp
; X64-NOT: chkstk
; X64: retq
define void @suppressed() "no-stack-arg-probe" {
  %p = alloca [8192 x i8], align 16
  call void @consume(ptr %p)
  ret void
}
; X32-LABEL: _sdiv64:
; X32: calll __alldiv{{$}}
; X32-NEXT: .cfi_adjust_cfa_offset -16
; X32-NEXT: retl
define i64 @sdiv64(i64 %a, i64 %b) {
  %r = sdiv i64 %a, %b
  ret i64 %r
}
; X32-LABEL: _udiv64:
; X32: calll __aulldiv{{$}}
; X32-NEXT: .cfi_adjust_cfa_offset -16
; X32-NEXT: retl
define i64 @udiv64(i64 %a, i64 %b) {
  %r = udiv i64 %a, %b
  ret i64 %r
}
; X32-LABEL: _srem64:
; X32: calll __allrem{{$}}
; X32-NEXT: .cfi_adjust_cfa_offset -16
; X32-NEXT: retl
define i64 @srem64(i64 %a, i64 %b) {
  %r = srem i64 %a, %b
  ret i64 %r
}
; X32-LABEL: _urem64:
; X32: calll __aullrem{{$}}
; X32-NEXT: .cfi_adjust_cfa_offset -16
; X32-NEXT: retl
define i64 @urem64(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}
; X32-LABEL: _custom:
; X32: calll _my_probe
; X32-NOT: __chkstk
; X32: retl
; X64-LABEL: custom:
; X64: callq my_probe
; X64-NOT: __chkstk
; X64: retq
define void @custom() "probe-stack"="my_probe" {
  %p = alloca [8192 x i8], align 16
  call void @consume(ptr %p)
  ret void
}
