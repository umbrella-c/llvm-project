// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
extern "C" void record_cleanup(int) noexcept;
struct DLLGuard {
  ~DLLGuard() { record_cleanup(6); }
};
extern "C" __declspec(dllexport) void dll_throw() {
  DLLGuard guard;
  throw 77;
}
extern "C" char __ImageBase;
extern "C" __declspec(dllexport) void *dll_image_base() { return &__ImageBase; }
