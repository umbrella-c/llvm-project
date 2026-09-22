// RUN: %clang_cc1 -triple x86_64-unknown-vali -fdeclspec -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i686-unknown-vali -fdeclspec -emit-llvm -o - %s | FileCheck %s

// libc++ declares extern instantiations in headers, then adds dllexport at
// their explicit definitions. The attribute must propagate to the members.
template <class T> struct Stream {
  void write() {}
};
extern template struct Stream<int>;
template struct __declspec(dllexport) Stream<int>;

// CHECK: define weak_odr {{.*}}dllexport {{.*}}void @_ZN6StreamIiE5writeEv(
