// REQUIRES: aarch64-registered-target
// RUN: %clang --target=aarch64-uml-vali -ffreestanding -fsyntax-only %s
// RUN: %clang --target=aarch64-uml-vali -ffreestanding -O1 -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang --target=aarch64-uml-vali -ffreestanding -O1 -S %s -o - | FileCheck %s --check-prefix=ASM

_Static_assert(sizeof(char) == 1 && (char)-1 < 0, "signed char");
_Static_assert(sizeof(short) == 2 && sizeof(int) == 4 && sizeof(long) == 4, "LLP64");
_Static_assert(sizeof(long long) == 8 && sizeof(void *) == 8, "64 bits");
_Static_assert(sizeof(__SIZE_TYPE__) == 8 && sizeof(__PTRDIFF_TYPE__) == 8, "pointer integers");
_Static_assert(sizeof(long double) == 8 && _Alignof(long double) == 8 &&
               __LDBL_MANT_DIG__ == 53, "binary64 long double");
_Static_assert(sizeof(__WCHAR_TYPE__) == 2 && (__WCHAR_TYPE__)-1 > 0, "wchar_t");
_Static_assert(__ARM_SIZEOF_WCHAR_T == 2, "ACLE wchar_t macro");
_Static_assert(sizeof(__builtin_va_list) == 32 && _Alignof(__builtin_va_list) == 8,
               "AAPCS64 va_list, not Windows va_list");
#ifndef VALI64
#error missing Vali macros
#endif
#ifdef _WIN32
#error must not impersonate Windows
#endif
struct pair { unsigned long long a, b; };
struct large { unsigned long long a, b, c; };
struct hfa { double a, b, c, d; };
// CHECK: target datalayout = "e-m:w-
// CHECK-LABEL: define{{.*}} [2 x i64] @pair_echo([2 x i64]
struct pair pair_echo(struct pair p) { return p; }
// CHECK-LABEL: define{{.*}} void @large_echo(ptr{{.*}}sret(%struct.large)
struct large large_echo(struct large p) { return p; }
// CHECK-LABEL: define{{.*}} %struct.hfa @hfa_echo([4 x double]
struct hfa hfa_echo(struct hfa p) { return p; }
double variadic(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  double d = __builtin_va_arg(ap, double);
  __builtin_va_end(ap);
  return d;
}
// FP varargs use SIMD registers and the AAPCS64 register save area.
// ASM-LABEL: variadic:
// ASM: stp q0, q1,
// ASM-LABEL: call_variadic:
// ASM: fmov d0, #1.00000000
// ASM: b variadic
// CHECK-LABEL: define{{.*}} double @call_variadic()
double call_variadic(void) { return variadic(1, 1.0); }
