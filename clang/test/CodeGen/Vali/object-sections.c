// REQUIRES: x86-registered-target
// RUN: %clang --target=i386-uml-vali -fembed-bitcode=all -c %s -o %t32.obj
// RUN: llvm-readobj --sections %t32.obj | FileCheck %s
// RUN: %clang --target=x86_64-uml-vali -fembed-bitcode=all -c %s -o %t64.obj
// RUN: llvm-readobj --sections %t64.obj | FileCheck %s
// CHECK-DAG: Name: cfstring
// CHECK-DAG: Name: .llvmbc
// CHECK-DAG: Name: .llvmcmd
const void *message = __builtin___CFStringMakeConstantString("Vali");
