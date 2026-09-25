// REQUIRES: x86-registered-target
// RUN: %clang --driver-mode=g++ -### --target=x86_64-uml-vali -static-libstdc++ -c %s 2>&1 | FileCheck %s --check-prefix=STATIC
// RUN: %clang --driver-mode=g++ -### --target=i686-uml-vali -static-libstdc++ -c %s 2>&1 | FileCheck %s --check-prefix=STATIC
// RUN: %clang --driver-mode=g++ -### --target=x86_64-uml-vali -static-libstdc++ -nostdinc++ %s 2>&1 | FileCheck %s --check-prefixes=STATIC,LINK
// RUN: %clang --driver-mode=g++ -### --target=x86_64-uml-vali -c %s 2>&1 | FileCheck %s --check-prefix=SHARED
// RUN: %clang --driver-mode=g++ -### --target=x86_64-uml-vali -static -static-libstdc++ -c %s 2>&1 | FileCheck %s --check-prefix=SHARED
// RUN: %clang --driver-mode=g++ -### --target=x86_64-pc-linux-gnu -static-libstdc++ -c %s 2>&1 | FileCheck %s --check-prefix=SHARED

// STATIC-NOT: argument unused during compilation
// STATIC: "-cc1"
// STATIC-SAME: "-D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS"
// STATIC-SAME: "-D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS"
// LINK: "c++.lib" "c++abi.lib" "unwind.dll.lib"
// SHARED: "-cc1"
// SHARED-NOT: -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS
// SHARED-NOT: -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS
int main() { return 0; }
