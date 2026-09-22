// RUN: %clang_cc1 -triple x86_64-unknown-vali -fdeclspec -fvisibility=hidden -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i686-unknown-vali -fdeclspec -fvisibility=hidden -emit-llvm -o - %s | FileCheck %s
struct __declspec(dllexport) Error {
  virtual ~Error();
};
Error::~Error() {}
// Exported RTTI must not acquire hidden visibility: LLVM rejects that IR.
// CHECK-DAG: @_ZTI5Error = {{.*}}dllexport {{(constant|global)}}
// CHECK-DAG: @_ZTS5Error = {{.*}}dllexport {{(constant|global)}}
