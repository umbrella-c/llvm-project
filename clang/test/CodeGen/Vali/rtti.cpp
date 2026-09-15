// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-llvm -o - %s | FileCheck %s --check-prefix=X64
// RUN: %clang_cc1 -triple i386-uml-vali -emit-llvm -o - %s | FileCheck %s --check-prefix=X32
// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-obj -o %t64.obj %s
// RUN: %clang_cc1 -triple i386-uml-vali -emit-obj -o %t32.obj %s

// Vali keeps Itanium RTTI, with pointer-sized base offsets on LLP64.
// X64: @_ZTI7Derived = linkonce_odr{{.*}} constant { ptr, ptr, i32, i32, ptr, i64, ptr, i64 }
// X32: @_ZTI7Derived = linkonce_odr{{.*}} constant { ptr, ptr, i32, i32, ptr, i32, ptr, i32 }
struct Left { virtual int left(); };
struct Right { virtual int right(); };
struct Derived : Left, Right { int left() override; };
int Derived::left() { return 42; }
