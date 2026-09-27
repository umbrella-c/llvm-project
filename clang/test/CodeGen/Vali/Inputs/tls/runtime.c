// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <internal/_tls.h>
#include <ddk/ddkdefs.h>

// Allocation and PE template preparation are fixture scaffolding. Access and
// logical-context switching use the actual Vali libc implementation.
unsigned long _tls_index = 3;
int *local_initialized(void);
int *local_zero(void);
char *local_aligned(void);
extern char template_start[], template_end[];
__declspec(dllimport) int *dll_value(void);
__declspec(dllimport) void dll_prepare(void *);
static thread_storage_t jobs[2];
static uint64_t workers[2][12];
static __attribute__((aligned(64))) char blocks[2][4096];
static __attribute__((aligned(64))) char dll_blocks[2][4096];

__attribute__((noreturn)) static void finish(int status) {
  register long x0 __asm__("x0") = status;
  register long x8 __asm__("x8") = 93;
  __asm__ volatile("svc #0" : : "r"(x0), "r"(x8) : "memory");
  __builtin_unreachable();
}
#define CHECK(x) do { if (!(x)) finish(__LINE__); } while (0)
static void worker(int n) {
  __asm__ volatile("msr tpidr_el0, %0" : : "r"(workers[n]) : "memory");
}
static void check_slots(int job, int unit) {
  CHECK(__tls_current() == &jobs[job]);
  CHECK(__get_reserved(1) == (uintptr_t)jobs[job].tls_array);
  CHECK(__get_reserved(11) == __get_reserved(1));
  CHECK(__get_reserved(2) == (uintptr_t)(100 + unit));
}
void entry(void) {
  CHECK(template_end - template_start < sizeof(blocks[0]));
  for (int j = 0; j < 2; ++j) {
    for (unsigned n = 0; n < template_end - template_start; ++n)
      blocks[j][n] = template_start[n];
    dll_prepare(dll_blocks[j]);
    jobs[j].tls_array[3] = (uintptr_t)blocks[j];
    jobs[j].tls_array[7] = (uintptr_t)dll_blocks[j];
    jobs[j].err_no = 10 + j;
    workers[j][2] = 100 + j;
  }
  worker(0);
  __tls_switch(&jobs[0]);
  check_slots(0, 0);
#ifdef INVALID_INDEX
  __vali_tls_get_block(64);
  finish(200);
#elif defined(MISSING_BLOCK)
  __vali_tls_get_block(5);
  finish(201);
#elif defined(MISSING_CONTEXT)
  __set_reserved(0, 0);
  __vali_tls_get_block(3);
  finish(202);
#endif
  CHECK(*local_initialized() == 41 && *local_zero() == 0 && *dll_value() == 73);
  CHECK(((uintptr_t)local_aligned() & 63) == 0);
  int *address = local_initialized(), *dll_address = dll_value();
  *local_initialized() = 101; *local_zero() = 102; local_aligned()[63] = 103; *dll_address = 104;
  __tls_switch(&jobs[1]);
  check_slots(1, 0);
  CHECK(__tls_current()->err_no == 11);
  CHECK(*local_initialized() == 41 && *local_zero() == 0 && local_aligned()[63] == 0);
  CHECK(local_initialized() != address && dll_value() != dll_address);
  CHECK(*dll_value() == 73);
  *local_initialized() = 201;
  // Migrate job 0 to another execution unit: its module addresses stay stable.
  worker(1);
  __tls_switch(&jobs[0]);
  check_slots(0, 1);
  CHECK(__tls_current()->err_no == 10);
  CHECK(local_initialized() == address && *local_initialized() == 101 && *local_zero() == 102);
  CHECK(local_aligned()[63] == 103 && dll_value() == dll_address && *dll_address == 104);
  worker(0);
  check_slots(1, 0);
  CHECK(*local_initialized() == 201);
  finish(0);
}
