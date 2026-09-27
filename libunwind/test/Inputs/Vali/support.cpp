// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include <exception>
#include <new>
#include <stdlib.h>
extern "C" void message(const char *);
extern "C" __attribute__((noreturn)) void __abort_message(const char *s, ...) {
  message(s);
  abort();
}
extern "C" {
std::terminate_handler __cxa_terminate_handler = abort;
std::unexpected_handler __cxa_unexpected_handler = abort;
std::new_handler __cxa_new_handler = nullptr;
}
void operator delete(void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void *operator new(size_t n) { return malloc(n); }
namespace std {
terminate_handler set_terminate(terminate_handler h) noexcept {
  auto old = __cxa_terminate_handler;
  __cxa_terminate_handler = h;
  return old;
}
} // namespace std

#include "support/runtime/exception_libcxxabi.ipp"
