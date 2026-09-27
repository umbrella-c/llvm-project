; REQUIRES: aarch64-registered-target
; RUN: llc -mtriple=aarch64-uml-vali %s -o - | FileCheck %s
; RUN: llc -mtriple=aarch64-uml-vali -filetype=obj %s -o %t.obj
; RUN: llvm-readobj --file-headers --relocations --unwind %t.obj | FileCheck %s --check-prefix=OBJ
; RUN: llc -mtriple=aarch64-pc-windows-msvc %s -o - | FileCheck %s --check-prefix=WINDOWS

@tls = thread_local global i32 17, align 4
@data = external dllimport global i32
declare dllimport i32 @imported(i32)
declare void @escape(ptr)

; CHECK-LABEL: tls_address:
; CHECK: adrp x{{[0-9]+}}, _tls_index
; CHECK: ldr w0,
; CHECK: bl __vali_tls_get_block
; CHECK: :secrel_hi12:tls
; CHECK: :secrel_lo12:tls
; CHECK-NOT: x18
; WINDOWS-LABEL: tls_address:
; WINDOWS: ldr x{{[0-9]+}}, [x18, #88]
; WINDOWS-NOT: __vali_tls_get_block
; OBJ: Format: COFF-ARM64
; OBJ: IMAGE_REL_ARM64_BRANCH26 __vali_tls_get_block
; OBJ: IMAGE_REL_ARM64_SECREL_HIGH12A tls
; OBJ: IMAGE_REL_ARM64_SECREL_LOW12A tls

define ptr @tls_address() uwtable {
  ret ptr @tls
}

; CHECK-LABEL: imports:
; CHECK: __imp_data
; CHECK: __imp_imported
; OBJ: __imp_data
; OBJ: __imp_imported

define i32 @imports() uwtable {
  %d = load i32, ptr @data
  %r = call i32 @imported(i32 %d)
  ret i32 %r
}

; CHECK-LABEL: large_frame:
; CHECK: mov x15,
; CHECK: bl __chkstk
; CHECK: .seh_stackalloc
; OBJ: IMAGE_REL_ARM64_BRANCH26 __chkstk
; OBJ: RuntimeFunction {

define void @large_frame() uwtable {
  %frame = alloca [32768 x i8], align 16
  call void @escape(ptr %frame)
  ret void
}

; No LSE or outline-atomic dependency in the bootstrap target.
; CHECK-LABEL: atomic_add:
; CHECK: ldaxr
; CHECK: stlxr
; CHECK-NOT: __aarch64_

define i64 @atomic_add(ptr %p) {
  %old = atomicrmw add ptr %p, i64 1 seq_cst
  ret i64 %old
}

; CHECK-LABEL: dynamic_frame:
; CHECK: bl __chkstk

define void @dynamic_frame(i64 %n) uwtable {
  %frame = alloca i8, i64 %n, align 16
  call void @escape(ptr %frame)
  ret void
}
