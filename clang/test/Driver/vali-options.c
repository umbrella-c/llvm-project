// REQUIRES: x86-registered-target
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/ambient %clang -### --target=x86_64-uml-vali --sysroot=%t/explicit -g -rdynamic %s -o %t.exe 2>&1 | FileCheck %s --check-prefix=ROOT
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali --driver-mode=g++ -nostdinc %s 2>&1 | FileCheck %s --check-prefix=NOINC
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali --driver-mode=g++ -nostdlibinc %s 2>&1 | FileCheck %s --check-prefix=NOLIBINC
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali --driver-mode=g++ -nodefaultlibs %s 2>&1 | FileCheck %s --check-prefix=NOLIBS
// RUN: env VALICC=%t/cross %clang -### --target=x86_64-uml-vali -g -g0 -nostdlib -e my_entry %s 2>&1 | FileCheck %s --check-prefix=ENTRY
// RUN: env VALICC=%t/cross %clang -### --target=x86_64-uml-vali -fuse-ld=lld %s 2>&1 | FileCheck %s --check-prefix=LLD
// RUN: not %clang -### --target=x86_64-uml-vali -fuse-ld=bfd %s 2>&1 | FileCheck %s --check-prefix=BADLD
// RUN: not %clang -### --target=mips-uml-vali %s 2>&1 | FileCheck %s --check-prefix=BADARCH
// RUN: not %clang -### --target=x86_64-uml-vali -fsanitize=address %s 2>&1 | FileCheck %s --check-prefix=ASAN

// RUN: env VALICC=%t/cross %clang -### --target=x86_64-uml-vali --driver-mode=g++ -rtlib=compiler-rt -unwindlib=none %s 2>&1 | FileCheck %s --check-prefix=NOUNWIND
// RUN: not %clang -### --target=x86_64-uml-vali -rtlib=libgcc %s 2>&1 | FileCheck %s --check-prefix=BADRT
// RUN: not %clang -### --target=x86_64-uml-vali --driver-mode=g++ -stdlib=libstdc++ %s 2>&1 | FileCheck %s --check-prefix=BADSTD

// RUN: env VALICC=%t/cross %clang -### --target=x86_64-uml-vali -O2 -Wa,-mbig-obj %s 2>&1 | FileCheck %s --check-prefix=DEFAULTS
// RUN: env VALICC=%t/cross %clang -### --target=x86_64-uml-vali -fno-ms-extensions %s 2>&1 | FileCheck %s --check-prefix=NOMS

// ROOT: "-cc1"
// ROOT-SAME: "{{[^"]*}}/explicit/include"
// ROOT-NOT: /ambient/
// ROOT: "{{[^"]*}}/cross/bin/lld-link"
// ROOT-SAME: "-entry:__CrtConsoleEntry" "-lldvpe" "-debug:dwarf"
// ROOT-SAME: "-libpath:{{[^"]*}}/explicit/lib"
// ROOT-NOT: "-dll"
// ROOT-NOT: /ambient/
// NOINC: "-cc1"
// NOINC-NOT: "-internal-isystem"
// NOINC: "{{[^"]*}}/cross/bin/lld-link"
// NOLIBINC: "-cc1"
// NOLIBINC-NOT: /sdk/include
// NOLIBINC: /include"
// NOLIBS: "{{[^"]*}}/cross/bin/lld-link"
// NOLIBS-NOT: c++.dll.lib
// NOLIBS-NOT: unwind.dll.lib
// NOLIBS-NOT: c.dll.lib
// NOLIBS-NOT: libcrt.lib
// ENTRY: "{{[^"]*}}/cross/bin/lld-link"
// ENTRY-SAME: "-entry:my_entry" "-lldvpe"
// ENTRY-NOT: -debug:dwarf
// ENTRY-NOT: __CrtConsoleEntry
// LLD: "{{[^"]*}}/cross/bin/lld-link"
// BADLD: error: invalid linker name
// BADARCH: error: unsupported option 'VPE linking' for target 'mips-uml-vali'
// ASAN: error: unsupported option '-fsanitize=address'

// NOUNWIND: "{{[^"]*}}/cross/bin/lld-link"
// NOUNWIND-SAME: "c++.dll.lib"
// NOUNWIND-NOT: unwind.dll.lib
// BADRT: error: unsupported option '-rtlib=libgcc'
// BADSTD: error: unsupported option '-stdlib=libstdc++'

// DEFAULTS: "-cc1"
// DEFAULTS-SAME: "-mframe-pointer=none"
// DEFAULTS-SAME: "-fms-extensions"
// DEFAULTS-SAME: "-faddrsig"
// DEFAULTS: "-out:a.run"
// NOMS: "-cc1"
// NOMS-NOT: "-fms-extensions"
// NOMS: "{{[^"]*}}/cross/bin/lld-link"
