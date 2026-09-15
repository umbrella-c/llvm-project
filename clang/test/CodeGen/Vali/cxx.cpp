// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i386-uml-vali -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-obj -o %t64.obj %s
// RUN: %clang_cc1 -triple i386-uml-vali -emit-obj -o %t32.obj %s

// C++ names, vtables and constructors retain the Itanium ABI.
// CHECK: @_ZTV4Base =
// CHECK: define{{.*}} @_ZN4Base3getEv(
struct Base {
  int value;
  virtual int get();
};
int Base::get() { return value; }

// CHECK: define{{.*}} @_ZN4ItemC2Ev(
struct Item { Item(); int value; };
Item::Item() : value(42) {}
