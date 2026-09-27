// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Single-threaded test platform. Linux syscalls are used only for diagnostic
// output and process exit. All tested code is compiled with the Vali ABI.
#include <os/unwind.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern char __ImageBase;
__declspec(dllimport) void *dll_image_base(void);
void *memcpy(void *d, const void *s, size_t n) {
  for (size_t i = 0; i < n; ++i)
    ((char *)d)[i] = ((const char *)s)[i];
  return d;
}
void *memmove(void *d, const void *s, size_t n) {
  if ((uintptr_t)d < (uintptr_t)s)
    return memcpy(d, s, n);
  while (n) {
    --n;
    ((char *)d)[n] = ((const char *)s)[n];
  }
  return d;
}
void *memset(void *d, int c, size_t n) {
  for (size_t i = 0; i < n; ++i)
    ((char *)d)[i] = (char)c;
  return d;
}
int memcmp(const void *a, const void *b, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (((const unsigned char *)a)[i] != ((const unsigned char *)b)[i])
      return ((const unsigned char *)a)[i] - ((const unsigned char *)b)[i];
  return 0;
}
size_t strlen(const char *p) {
  size_t n = 0;
  while (p[n])
    ++n;
  return n;
}
int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return (unsigned char)*a - (unsigned char)*b;
}
void message(const char *p) {
  register uint64_t x0 __asm__("x0") = 1;
  register const char *x1 __asm__("x1") = p;
  register size_t x2 __asm__("x2") = strlen(p);
  register uint64_t x8 __asm__("x8") = 64;
  __asm__ volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
}
__attribute__((noreturn)) void test_exit(int code) {
  register uint64_t x0 __asm__("x0") = (unsigned)code;
  register uint64_t x8 __asm__("x8") = 93;
  __asm__ volatile("svc #0" : : "r"(x0), "r"(x8) : "memory");
  __builtin_unreachable();
}
__attribute__((noreturn)) void abort(void) {
  message("ABORT\n");
  test_exit(99);
}
__attribute__((noreturn)) void _assert_panic(const char *s, ...) {
  message(s);
  abort();
}
void *__get_std_handle(int n) {
  (void)n;
  return 0;
}
int fprintf(void *f, const char *fmt, ...) {
  (void)f;
  message(fmt);
  return 0;
}
int fflush(void *f) {
  (void)f;
  return 0;
}
char *getenv(const char *p) {
  (void)p;
  return 0;
}

static _Alignas(16) unsigned char heap[1024 * 1024];
static size_t used;
void *malloc(size_t n) {
  n = (n + 15) & ~(size_t)15;
  if (n > sizeof(heap) - used)
    abort();
  void *p = heap + used;
  used += n;
  return p;
}
void free(void *p) { (void)p; }
void *calloc(size_t n, size_t s) {
  void *p = malloc(n * s);
  return memset(p, 0, n * s);
}
void *aligned_alloc(size_t alignment, size_t n) {
  if (alignment > 16)
    abort();
  return malloc(n);
}

// Replace only the process-service transport. The test compiles Vali's actual
// libos/unwind.c, so module selection and exception-directory lookup are
// tested.
void sys_process_get_modules(void *c, void *m, int p) {
  (void)c;
  (void)m;
  (void)p;
}
void gracht_client_await(void *c, void *m, int p) {
  (void)c;
  (void)m;
  (void)p;
}
void sys_process_get_modules_result(void *c, void *m, uintptr_t *modules,
                                    uint32_t *capacity, int *count) {
  (void)c;
  (void)m;
  if (*capacity < 2)
    abort();
  modules[0] = (uintptr_t)&__ImageBase;
  modules[1] = (uintptr_t)dll_image_base();
  *count = 2;
}
int strncmp(const char *a, const char *b, size_t n) {
  while (n) {
    if (*a != *b || !*a)
      return (unsigned char)*a - (unsigned char)*b;
    ++a;
    ++b;
    --n;
  }
  return 0;
}

// Exercise the real module-lookup edge cases without an operating-system boot.
int test_module_lookup(void) {
  unsigned char *base = (unsigned char *)&__ImageBase;
  uint32_t nt = *(uint32_t *)(base + 0x3c);
  uint32_t *directories = (uint32_t *)(base + nt + 24 + 108);
  uint32_t *rva = (uint32_t *)(base + nt + 24 + 136);
  uint32_t *size = rva + 1;
  uint32_t savedDirectories = *directories, savedRVA = *rva, savedSize = *size;
  UnwindSection_t s;
  int ok = UnwindGetSection((void *)test_module_lookup, &s) == OS_EOK &&
           s.ModuleBase == base && s.UnwindSectionBase == base + *rva &&
           s.UnwindSectionLength == *size;
  *rva = *size = 0;
  ok &= UnwindGetSection((void *)test_module_lookup, &s) == OS_EOK &&
        s.ModuleBase == base && !s.UnwindSectionBase && !s.UnwindSectionLength;
  *directories = 0;
  ok &= UnwindGetSection((void *)test_module_lookup, &s) == OS_EOK &&
        !s.UnwindSectionLength;
  *directories = savedDirectories;
  *rva = savedRVA;
  *size = 7;
  ok &= UnwindGetSection((void *)test_module_lookup, &s) == OS_ENOENT;
  *size = savedSize;
  ok &= UnwindGetSection((void *)1, &s) == OS_ENOENT;
  return ok;
}
