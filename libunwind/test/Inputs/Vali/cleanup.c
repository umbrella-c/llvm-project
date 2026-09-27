// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
extern void throw_int(void);
extern void record_cleanup(int);
static void cleanup(int *n) { record_cleanup(*n); }
__attribute__((noinline)) void c_cleanup(void) {
  int n __attribute__((cleanup(cleanup))) = 3;
  throw_int();
}
