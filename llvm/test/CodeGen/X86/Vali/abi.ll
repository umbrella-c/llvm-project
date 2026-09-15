; RUN: llc -mtriple=x86_64-uml-vali -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=X64 --implicit-check-not=.seh
; RUN: llc -mtriple=i386-uml-vali -verify-machineinstrs %s -o - | FileCheck %s --check-prefix=X32
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t64.o
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.o
; RUN: llvm-readobj --file-headers %t64.o | FileCheck %s --check-prefix=OBJ64
; RUN: llvm-readobj --file-headers %t32.o | FileCheck %s --check-prefix=OBJ32
; OBJ64: Format: COFF-x86-64
; OBJ32: Format: COFF-i386

define i64 @fifth(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e) {
  ret i64 %e
}
; X64-LABEL: fifth:
; X64: movq 40(%rsp), %rax
; X64: retq

define double @mixed(i64 %a, double %b) {
  ret double %b
}
; X64-LABEL: mixed:
; X64: movaps %xmm1, %xmm0
; X64: retq

declare i64 @callee(i64, i64, i64, i64, i64)
define i64 @caller() uwtable {
  %r = call i64 @callee(i64 1, i64 2, i64 3, i64 4, i64 5)
  ret i64 %r
}
; X64-LABEL: caller:
; X64: subq $40, %rsp
; X64-DAG: movl $1, %ecx
; X64-DAG: movl $2, %edx
; X64-DAG: movl $3, %r8d
; X64-DAG: movl $4, %r9d
; X64-DAG: movq $5, 32(%rsp)
; X64: callq callee
; X64: addq $40, %rsp

; Explicit SysV must remain an opt-in override on Vali.
define x86_64_sysvcc i64 @sysv(i64 %a) {
  ret i64 %a
}
; X64-LABEL: sysv:
; X64: movq %rdi, %rax

define x86_stdcallcc i32 @stdcall(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}
; X32-LABEL: _stdcall@8:
; X32: retl $8

define x86_fastcallcc i32 @fastcall(i32 inreg %a, i32 inreg %b) {
  %r = add i32 %a, %b
  ret i32 %r
}
; X32-LABEL: @fastcall@8:
; X32: leal (%ecx,%edx), %eax
; X32: retl

; Vali must preserve the Win64 nonvolatile register set without SEH tables.
define void @preserve() uwtable {
  call void asm sideeffect "", "~{rdi},~{xmm6}"()
  ret void
}
; X64-LABEL: preserve:
; X64: pushq %rdi
; X64: movaps %xmm6,
; X64: movaps {{.*}}, %xmm6
; X64: popq %rdi

declare void @llvm.va_start(ptr)
declare void @llvm.va_end(ptr)
define i64 @vararg(i64 %fixed, ...) uwtable {
  %ap = alloca ptr, align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, i64
  call void @llvm.va_end(ptr %ap)
  ret i64 %v
}
; X64-LABEL: vararg:
; X64-DAG: movq %rdx,
; X64-DAG: movq %r8,
; X64-DAG: movq %r9,
