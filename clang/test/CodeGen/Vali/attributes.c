// RUN: %clang_cc1 -triple x86_64-uml-vali -fms-extensions -mstack-probe-size=8192 -mno-stack-arg-probe -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i386-uml-vali -fms-extensions -mstack-probe-size=8192 -mno-stack-arg-probe -emit-llvm -o - %s | FileCheck %s

#pragma comment(lib, "runtime")
#pragma detect_mismatch("abi", "vali")

void __attribute__((force_align_arg_pointer)) aligned(void) {}

// CHECK: attributes #{{[0-9]+}} = {{.*}}"no-stack-arg-probe"{{.*}}"stack-probe-size"="8192"{{.*}}"stackrealign"
// CHECK: !{!"-lruntime"}
// CHECK-NOT: /FAILIFMISMATCH
