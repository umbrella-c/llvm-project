// REQUIRES: x86-registered-target
// RUN: env VALICC=%t/cross CROSS=%t/legacy VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali %s -L%t/extra -lwidget -o %t.exe 2>&1 | FileCheck %s --check-prefixes=COMMON,EXE
// RUN: env VALICC=%t/cross CROSS=%t/legacy VALI_SDK_PATH=%t/sdk %clang -### --target=i386-uml-vali %s -L%t/extra -lwidget -o %t.exe 2>&1 | FileCheck %s --check-prefixes=COMMON,EXE
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali -shared %s -o %t.dll 2>&1 | FileCheck %s --check-prefix=DLL
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali --driver-mode=g++ %s -o %t.exe 2>&1 | FileCheck %s --check-prefix=CXX
// RUN: env VALICC=%t/cross VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali -nostdlib %s -o %t.exe 2>&1 | FileCheck %s --check-prefix=NOSTDLIB
// RUN: env -u VALICC CROSS=%t/legacy VALI_SDK_PATH=%t/sdk %clang -### --target=x86_64-uml-vali %s -o %t.exe 2>&1 | FileCheck %s --check-prefix=LEGACY

// COMMON: "-cc1"
// COMMON-SAME: "-triple" "{{(x86_64|i386)}}-uml-vali"
// COMMON-SAME: "{{[^"]*}}/sdk/include"
// EXE: "{{[^"]*}}/cross/bin/lld-link"
// EXE-SAME: "-entry:__CrtConsoleEntry"
// EXE-SAME: "-lldvpe"
// EXE-SAME: "-libpath:{{[^"]*}}/sdk/lib"
// EXE-SAME: "-libpath:{{[^"]*}}/extra"
// EXE-SAME: "widget.lib"
// EXE-SAME: "c.dll.lib" "m.dll.lib" "librt.lib" "libcrt.lib"

// DLL: "{{[^"]*}}/cross/bin/lld-link"
// DLL-SAME: "-dll"
// DLL-SAME: "-implib:{{[^"]*}}.dll.lib"
// DLL-SAME: "-entry:__CrtLibraryEntry"
// DLL-SAME: "-lldvpe"

// CXX: "{{[^"]*}}/sdk/include/c++/v1"
// CXX: "{{[^"]*}}/cross/bin/lld-link"
// CXX-SAME: "c++.dll.lib" "c++abi.dll.lib" "unwind.dll.lib"

// NOSTDLIB: "{{[^"]*}}/cross/bin/lld-link"
// NOSTDLIB-NOT: -entry:
// NOSTDLIB: "-lldvpe"
// NOSTDLIB-NOT: "c.dll.lib"
// NOSTDLIB-NOT: "m.dll.lib"
// NOSTDLIB-NOT: "librt.lib"
// NOSTDLIB-NOT: "libcrt.lib"

// LEGACY: "{{[^"]*}}/legacy/bin/lld-link"
