// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
unsigned long _tls_index = 7;
__thread int value = 73;
extern char template_start[], template_end[];
__declspec(dllexport) int *dll_value(void) { return &value; }
__declspec(dllexport) void dll_prepare(void *block) {
  char *p = block;
  for (unsigned n = 0; n < template_end - template_start; ++n)
    p[n] = template_start[n];
}
