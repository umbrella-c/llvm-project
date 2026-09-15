//===----------------------------------------------------------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
// REQUIRES: target={{.+-vali.*}}
#include <typeinfo>
#if _LIBCPP_TYPEINFO_COMPARISON_IMPLEMENTATION != 2
# error Vali needs non-unique RTTI comparison across images
#endif
struct Info : std::type_info { explicit Info(const char* name) : type_info(name) {} };
int main(int, char**) {
  char first[] = "7Example", second[] = "7Example", different[] = "5Other";
  Info a(first), b(second), c(different);
  if (!(a == b) || a == c) return 1;
  if (a.hash_code() != b.hash_code()) return 2;
  if (a.before(b) || b.before(a)) return 3;
  return 0;
}
