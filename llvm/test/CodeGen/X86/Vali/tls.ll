; RUN: llc -O0 -mtriple=i386-uml-vali -filetype=obj %s -o %t-o0-32.obj
; RUN: llc -O0 -mtriple=x86_64-uml-vali -filetype=obj %s -o %t-o0-64.obj
; RUN: llc -mtriple=i386-uml-vali %s -o - | FileCheck %s --check-prefix=X32
; RUN: llc -mtriple=x86_64-uml-vali %s -o - | FileCheck %s --check-prefix=X64
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.obj
; RUN: llvm-readobj --relocations --sections %t32.obj | FileCheck %s --check-prefix=OBJ32
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t64.obj
; RUN: llvm-readobj --relocations --sections %t64.obj | FileCheck %s --check-prefix=OBJ64
;
; Vali's __tls_switch publishes the TLS array at GS reserved slot 11.
; The CRT assigns a module index at runtime, even for local-exec variables.
; OBJ32: Name: .tls$
; OBJ32: IMAGE_REL_I386_DIR32 __tls_index
; OBJ32: IMAGE_REL_I386_SECREL _initialized
; OBJ64: Name: .tls$
; OBJ64: IMAGE_REL_AMD64_REL32 _tls_index
; OBJ64: IMAGE_REL_AMD64_SECREL initialized

@initialized = thread_local global i32 42, align 4
@zero = thread_local global i32 0, align 4
@local = thread_local(localexec) global i32 7, align 4

; X32-LABEL: _address:
; X32-DAG: movl %gs:44,
; X32-DAG: movl __tls_index,
; X32: initialized@SECREL32
; X32: retl
; X64-LABEL: address:
; X64-DAG: movq %gs:88,
; X64-DAG: movl _tls_index(%rip),
; X64: initialized@SECREL32
; X64: retq
define ptr @address() {
  ret ptr @initialized
}

; X32-LABEL: _local_address:
; X32-DAG: movl %gs:44,
; X32-DAG: movl __tls_index,
; X32: local@SECREL32
; X32: retl
; X64-LABEL: local_address:
; X64-DAG: movq %gs:88,
; X64-DAG: movl _tls_index(%rip),
; X64: local@SECREL32
; X64: retq
define ptr @local_address() {
  ret ptr @local
}

; X32-LABEL: _update:
; X32: zero@SECREL32
; X32: retl
; X64-LABEL: update:
; X64: zero@SECREL32
; X64: retq
define void @update(i32 %value) {
  store i32 %value, ptr @zero, align 4
  ret void
}
