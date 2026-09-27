//===----------------------------------------------------------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

// The decoder is address-space independent, so test malformed metadata and
// partial frames on the build host as well as executing real PE code on ARM64.
// RUN: %{cxx} %{flags} %{compile_flags} -std=c++11 %s -o %t.exe %{link_flags}
// RUN: %{exec} %t.exe

#include "../src/UnwindPEArm64.hpp"
#undef NDEBUG
#include <cassert>
#include <cstring>

struct Memory {
  using pint_t = uintptr_t;
  template <class T> T get(uintptr_t p) {
    T v;
    memcpy(&v, (void *)p, sizeof(v));
    return v;
  }
  uint8_t get8(uintptr_t p) { return get<uint8_t>(p); }
  uint16_t get16(uintptr_t p) { return get<uint16_t>(p); }
  uint32_t get32(uintptr_t p) { return get<uint32_t>(p); }
  uint64_t get64(uintptr_t p) { return get<uint64_t>(p); }
  double getDouble(uintptr_t p) { return get<double>(p); }
};
struct Registers {
  uint64_t x[32] = {}, pc = 0;
  double d[32] = {};
  uint64_t getSP() const { return x[31]; }
  void setSP(uint64_t n) { x[31] = n; }
  uint64_t getIP() const { return pc; }
  void setIP(uint64_t n) { pc = n; }
  uint64_t getFP() const { return x[29]; }
  uint64_t getRegister(int r) const { return x[r]; }
  void setRegister(int r, uint64_t n) { x[r] = n; }
  void setFloatRegister(int r, double n) { d[r - 64] = n; }
};
using Decoder = libunwind::UnwindPEArm64<Memory>;
struct Fixture {
  alignas(16) uint8_t image[4096] = {};
  alignas(16) uint64_t stack[2048] = {};
  Memory memory;
  Decoder decoder{memory, uintptr_t(image), sizeof(image)};
  Decoder::Info info;
  Registers regs;
  Fixture() {
    regs.setSP(uintptr_t(stack));
    regs.pc = 0x4040;
    regs.x[30] = 0x1234;
    put(0x200, 0x400);
    put(0x204, 0x300);
    // PE32+ header, one executable section and exception directory.
    put(0, 0x5a4d);
    put(0x3c, 0x80);
    put(0x80, 0x4550);
    put(0x84, 0x1aa64);
    put(0x94, 240);
    put(0x98, 0x20b);
    put(0x98 + 108, 16);
    put(0x98 + 136, 0x200);
    put(0x98 + 140, 8);
    put(0x188 + 8, 0x100);
    put(0x188 + 12, 0x400);
    put(0x188 + 36, 0x20000000);
  }
  void put(unsigned p, uint32_t v) { memcpy(image + p, &v, 4); }
  void codes(const uint8_t *c, unsigned n, unsigned epi = 0) {
    unsigned words = (n + 3) / 4;
    put(0x300, 32 | (words << 27) | (epi ? (1 << 21) | (epi << 22) : 0));
    memcpy(image + 0x304, c, n);
    assert(decoder.read(0x200, info));
  }
  bool step(unsigned offset = 32) {
    return decoder.step(info, 0x400 + offset, regs);
  }
};

int main(int, char **) {
  { // Full record: x19/x20 and FP/LR, frame-pointer recovery after alloca.
    Fixture f;
    const uint8_t code[] = {0xe1, 0x42, 0x24, 0xe4};
    f.codes(code, sizeof(code));
    f.stack[0] = 19;
    f.stack[1] = 20;
    f.stack[2] = 29;
    f.stack[3] = 30;
    f.regs.x[29] = uintptr_t(f.stack);
    f.regs.setSP(uintptr_t(f.stack) - 64);
    assert(f.step());
    assert(f.regs.x[19] == 19 && f.regs.x[20] == 20 && f.regs.x[29] == 29);
    assert(f.regs.pc == 30 && f.regs.getSP() == uintptr_t(f.stack + 4));
    assert(f.decoder.find(0x410, f.info) == 1);
    assert(f.decoder.find(0x490, f.info) == 0);  // executable leaf
    assert(f.decoder.find(0x210, f.info) == -1); // data is not a leaf
    f.put(0x204, 4092);
    f.put(4092, 32 | (31u << 27));
    assert(!f.decoder.read(0x200, f.info)); // truncated xdata
  }
  { // Partial prologue: only the first store has executed.
    Fixture f;
    const uint8_t c[] = {0xe1, 0x42, 0x24, 0xe4};
    f.codes(c, sizeof(c));
    f.stack[0] = 19;
    f.stack[1] = 20;
    assert(f.step(4));
    assert(f.regs.x[19] == 19 && f.regs.pc == 0x1234);
    assert(f.regs.getSP() == uintptr_t(f.stack + 4));
  }
  { // save_next is stored before its base save (reverse prologue order).
    Fixture f;
    const uint8_t c[] = {0xe6, 0xe6, 0x28, 0xe4};
    f.codes(c, sizeof(c));
    for (unsigned i = 0; i < 6; ++i)
      f.stack[i] = i + 19;
    assert(f.step());
    for (unsigned i = 0; i < 6; ++i)
      assert(f.regs.x[i + 19] == i + 19);
    assert(f.regs.getSP() == uintptr_t(f.stack + 8));
  }
  { // FP pair and FP single; integer and floating-point state stay separate.
    Fixture f;
    const uint8_t c[] = {0xdc, 0x84, 0xda, 0x03, 0xe4};
    f.codes(c, sizeof(c));
    double a = 8.5, b = 9.5, c10 = 10.5;
    memcpy(f.stack, &a, 8);
    memcpy(f.stack + 1, &b, 8);
    memcpy(f.stack + 4, &c10, 8);
    assert(f.step());
    assert(f.regs.d[8] == a && f.regs.d[9] == b && f.regs.d[10] == c10);
  }
  { // Explicit epilogue scope, already restored FP/LR.
    Fixture f;
    const uint8_t c[] = {0x42, 0x24, 0xe4};
    f.codes(c, sizeof(c));
    f.put(0x300, 32 | (1 << 22) | (1 << 27));
    f.put(0x304, 20);
    memcpy(f.image + 0x308, c, sizeof(c));
    assert(f.decoder.read(0x200, f.info));
    f.stack[0] = 19;
    f.stack[1] = 20;
    assert(f.step(84));
    assert(f.regs.pc == 0x1234 && f.regs.x[19] == 19);
  }
  { // Single epilogue index, skip the first restore at the second instruction.
    Fixture f;
    const uint8_t c[] = {0xe3, 0x42, 0x24, 0xe4};
    f.codes(c, sizeof(c), 1);
    f.stack[0] = 19;
    f.stack[1] = 20;
    assert(f.step(120));
    assert(f.regs.pc == 0x1234 && f.regs.x[19] == 19);
  }
  { // Packed canonical frame with odd RegI and LR in the same pair.
    Fixture f;
    f.put(0x204, 1 | (32 << 2) | (3 << 16) | (1 << 21) | (4 << 23));
    assert(f.decoder.read(0x200, f.info));
    for (unsigned i = 0; i < 4; ++i)
      f.stack[4 + i] = i + 19;
    assert(f.step());
    assert(f.regs.x[21] == 21 && f.regs.pc == 22);
    assert(f.regs.getSP() == uintptr_t(f.stack + 8));
  }
  { // Packed FP-only frame.
    Fixture f;
    f.put(0x204, 1 | (32 << 2) | (1 << 13) | (1 << 23));
    assert(f.decoder.read(0x200, f.info));
    double a = 8.5;
    memcpy(f.stack, &a, 8);
    assert(f.step());
    assert(f.regs.d[8] == a);
  }
  { // Handler RVA and LSDA immediately following unwind bytes.
    Fixture f;
    f.put(0x300, 32 | (1 << 20) | (1 << 27));
    f.put(0x304, 0xe4);
    f.put(0x308, 0x480);
    assert(f.decoder.read(0x200, f.info));
    assert(f.info.handler == 0x480 && f.info.lsda == 0x30c);
  }
  { // Chained phantom prologue isn't counted in this fragment.
    Fixture f;
    const uint8_t c[] = {0xe5, 0x24, 0xe4};
    f.codes(c, sizeof(c));
    f.stack[0] = 19;
    assert(f.step(0));
    assert(f.regs.x[19] == 19);
  }
  { // Unknown/versioned/PAC/SVE/malformed codes must not modify a cursor.
    const uint8_t bad[] = {0xff, 0xfc, 0xdf, 0xe6, 0xe8};
    for (auto b : bad) {
      Fixture f;
      uint8_t c[] = {b, 0xe4};
      f.codes(c, sizeof(c));
      auto before = f.regs;
      assert(!f.step());
      assert(memcmp(&before, &f.regs, sizeof(before)) == 0);
    }
    Fixture f;
    f.put(0x300, 32 | (1 << 18));
    assert(!f.decoder.read(0x200, f.info));
  }
  return 0;
}
