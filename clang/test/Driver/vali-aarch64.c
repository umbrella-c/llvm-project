// REQUIRES: aarch64-registered-target
// RUN: %clang --target=aarch64-uml-vali -### -nostdlib %s -o %t.exe 2>&1 | FileCheck %s
// CHECK: "-triple" "aarch64-uml-vali"
// CHECK-SAME: "-mrelocation-model" "pic"
// CHECK-NOT: "-fno-signed-char"
// CHECK-NOT: "+outline-atomics"
// CHECK: lld-link
// CHECK-SAME: "-lldvpe"
// CHECK-NOT: "c.dll.lib"
// CHECK-NOT: "libcrt.lib"

// RUN: not %clang --target=aarch64-uml-vali -fno-integrated-as -### -c %s 2>&1 | FileCheck %s --check-prefix=EXTERNAL
// EXTERNAL: error: unsupported option 'external assembly' for target 'aarch64-uml-vali'
