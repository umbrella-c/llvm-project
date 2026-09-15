; RUN: llc -mtriple=x86_64-uml-vali -verify-machineinstrs %s -o %t.s
; RUN: FileCheck %s < %t.s
; RUN: llc -mtriple=i386-uml-vali -verify-machineinstrs %s -o %t32.s
; RUN: FileCheck %s < %t32.s
; RUN: llc -mtriple=i386-uml-vali -filetype=obj %s -o %t32.o
; RUN: llc -mtriple=x86_64-uml-vali -filetype=obj %s -o %t.o
; RUN: llvm-readobj --sections --relocations %t.o | FileCheck %s --check-prefix=OBJ
@imported = external dllimport global i32
@external = external global i32
@exported = dllexport global i32 7
@llvm.global_ctors = appending global [4 x {i32, ptr, ptr}] [
  {i32, ptr, ptr} {i32 1, ptr @init, ptr null},
  {i32, ptr, ptr} {i32 200, ptr @init, ptr null},
  {i32, ptr, ptr} {i32 400, ptr @init, ptr null},
  {i32, ptr, ptr} {i32 65535, ptr @init, ptr null}]
@llvm.global_dtors = appending global [1 x {i32, ptr, ptr}] [
  {i32, ptr, ptr} {i32 65535, ptr @init, ptr null}]
define internal void @init() { ret void }
define i32 @read_imported() {
  %v = load i32, ptr @imported
  ret i32 %v
}
define i32 @read_external() {
  %v = load i32, ptr @external
  ret i32 %v
}
; CHECK: __imp_{{_?}}imported
; CHECK: .refptr.{{_?}}external
; CHECK: .CRT$XCA00001
; CHECK: .CRT$XCT00200
; CHECK: .CRT$XCT00400
; CHECK: .CRT$XCU
; CHECK: .CRT$XTX
; CHECK: -export:{{_?}}exported,data
; OBJ: IMAGE_REL_AMD64_REL32 __imp_imported
; OBJ: IMAGE_REL_AMD64_REL32 .refptr.external
; OBJ: IMAGE_REL_AMD64_ADDR64 external
