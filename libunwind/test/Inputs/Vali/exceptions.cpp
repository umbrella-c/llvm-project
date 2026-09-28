// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <exception>
#include <stdarg.h>
#include <stdint.h>
#include <unwind.h>
extern "C" {
void message(const char *);
[[noreturn]] void test_exit(int);
void c_cleanup();
int test_module_lookup();
__declspec(dllimport) void dll_throw();
}
static int order;
extern "C" void record_cleanup(int n) { order = order * 10 + n; }
struct Guard {
  int n;
  ~Guard() { record_cleanup(n); }
};
extern "C" __attribute__((noinline)) void throw_int() {
  // Force preservation of all AAPCS64 nonvolatile FP and several GP registers.
  __asm__ volatile("fmov d8, xzr\n\tmov x19, xzr" ::
                       : "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
                         "x19", "x20", "x21", "x22", "x23", "x24", "x25",
                         "x26");
  throw 42;
}
__attribute__((noinline)) static void large(int n) {
  volatile char fixed[20000];
  volatile char *dynamic = (volatile char *)__builtin_alloca(n);
  fixed[0] = 4;
  dynamic[0] = 2;
  __asm__ volatile("" : : "r"(fixed), "r"(dynamic) : "memory");
  Guard g{2};
  c_cleanup();
}
__attribute__((noinline)) static void variadic(int n, ...) {
  va_list ap;
  va_start(ap, n);
  double d = va_arg(ap, double);
  va_end(ap);
  if (d != 1.5)
    test_exit(10);
  Guard g{1};
  large(n);
}
struct Base {
  virtual ~Base() {}
  int n = 17;
};
struct Derived : Base {
  int m = 23;
};
__attribute__((noinline)) static void throw_derived() { throw Derived{}; }
static _Unwind_Exception forced;
static _Unwind_Reason_Code stop(int, _Unwind_Action actions, uint64_t,
                                _Unwind_Exception *, _Unwind_Context *,
                                void *) {
  if (actions & _UA_END_OF_STACK) {
    if (order != 54)
      test_exit(17);
    message("PASS Vali ARM64 exceptions\n");
    test_exit(0);
  }
  return _URC_NO_REASON;
}
__attribute__((noinline)) static void force() {
  Guard g{5};
  _Unwind_ForcedUnwind(&forced, stop, nullptr);
  test_exit(18);
}
#ifdef TEST_THREADED_TLS
extern "C" void threaded_setup();
extern "C" void threaded_verify();
#endif
extern "C" void entry() {
#ifdef TEST_THREADED_TLS
  threaded_setup();
  threaded_verify();
#endif
  if (!test_module_lookup())
    test_exit(11);
  register double saved __asm__("d8") = 123.5;
  register uint64_t integer __asm__("x19") = 0xdeadbeef12345678ULL;
  __asm__ volatile("" : "+w"(saved), "+r"(integer));
  try {
    variadic(1024, 1.5);
    test_exit(1);
  } catch (int n) {
    if (n != 42 || order != 321)
      test_exit(2);
  }
  __asm__ volatile("" : "+w"(saved), "+r"(integer));
  if (saved != 123.5 || integer != 0xdeadbeef12345678ULL)
    test_exit(3);
  try {
    try {
      throw_int();
    } catch (int n) {
      if (n != 42)
        test_exit(4);
      throw;
    }
  } catch (int n) {
    if (n != 42)
      test_exit(5);
  }
  try {
    throw_derived();
  } catch (const Base &b) {
    if (b.n != 17)
      test_exit(6);
  }
  try {
    throw 2.25;
  } catch (double d) {
    if (d != 2.25)
      test_exit(7);
  }
  order = 0;
  try {
    dll_throw();
  } catch (int n) {
    if (n != 77 || order != 6)
      test_exit(9);
  }
  if (std::uncaught_exceptions() != 0)
    test_exit(8);
  message("throw/catch, C cleanup, RAII, rethrow, FP/GP, large frames, DLL: "
          "PASS\n");
  order = 0;
  Guard g{4};
  force();
}
