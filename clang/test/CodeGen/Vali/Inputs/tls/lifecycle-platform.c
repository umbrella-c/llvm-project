#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
int live_allocations, fail_allocation = -1;
static _Alignas(16) char heap[1024 * 1024];
static size_t used;
__attribute__((noreturn)) void test_exit(int status) {
  register long x0 __asm__("x0") = status;
  register long x8 __asm__("x8") = 93;
  __asm__ volatile("svc #0" : : "r"(x0), "r"(x8) : "memory");
  __builtin_unreachable();
}
void *malloc(size_t size) {
  if (fail_allocation == 0) return 0;
  if (fail_allocation > 0) --fail_allocation;
  size = (size + 15) & ~(size_t)15;
  if (size > sizeof(heap) - used) __builtin_trap();
  void *p = heap + used; used += size; ++live_allocations; return p;
}
void free(void *p) { if (p) --live_allocations; }
void *memcpy(void *dst, const void *src, size_t n) {
  for (size_t i = 0; i < n; ++i) ((char*)dst)[i] = ((const char*)src)[i];
  return dst;
}
void *memset(void *dst, int value, size_t n) {
  for (size_t i = 0; i < n; ++i) ((char*)dst)[i] = value;
  return dst;
}
void _fpreset(void) {}

#include <internal/_tls.h>
#include <stdlib.h>
void *dsalloc(size_t n) { return malloc(n); }
void dsfree(void *p) { free(p); }
int *__errno(void) { return &__tls_current()->err_no; }
uuid_t __crt_thread_id(void) { return __tls_current()->job_id; }
