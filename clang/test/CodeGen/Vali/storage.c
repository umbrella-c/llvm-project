// RUN: %clang_cc1 -triple x86_64-uml-vali -fno-auto-import -emit-llvm -o - %s | FileCheck %s --check-prefix=NOIMPORT
// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple i386-uml-vali -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-uml-vali -debug-info-kind=standalone -dwarf-version=4 -emit-obj -o %t64.obj %s
// RUN: llvm-readobj --sections --relocations %t64.obj | FileCheck %s --check-prefix=OBJ64
// RUN: %clang_cc1 -triple i386-uml-vali -debug-info-kind=standalone -dwarf-version=4 -emit-obj -o %t32.obj %s
// RUN: llvm-readobj --sections --relocations %t32.obj | FileCheck %s --check-prefix=OBJ32

// CHECK: @local_tls = dso_local thread_local global i32 42
// CHECK: @external_data = external global i32
// CHECK: @weak_data = extern_weak global i32
_Thread_local int local_tls = 42;
extern int external_data;
extern int weak_data __attribute__((weak));

int read_data(void) { return local_tls + external_data + weak_data; }

// OBJ64: Name: .tls$
// OBJ64: Name: .debug_info
// OBJ64: IMAGE_REL_AMD64_SECREL local_tls
// OBJ32: Name: .tls$
// OBJ32: Name: .debug_info
// OBJ32: IMAGE_REL_I386_SECREL _local_tls

// NOIMPORT: @external_data = external dso_local global i32
// NOIMPORT: @weak_data = extern_weak global i32
