//===-- UnwindPEArm64.hpp - Vali PE/ARM64 unwinding ----------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __UNWIND_PE_ARM64_HPP__
#define __UNWIND_PE_ARM64_HPP__

#include <stdint.h>

namespace libunwind {

// PE is only the frame-description format here. The handler is an Itanium
// personality, and the bytes following its RVA are the Itanium LSDA. No Windows
// CONTEXT, dispatcher, TEB or RtlVirtualUnwind ABI is involved.
//
// Format reference:
// https://learn.microsoft.com/cpp/build/arm64-exception-handling
// The initial port supports ARMv8-A user frames. PAC, SVE, full-Q saves and
// Windows trap/context records fail closed instead of returning a wrong frame.
template <class A> class UnwindPEArm64 {
  typedef typename A::pint_t pint_t;
  A &as;
  pint_t base;
  uint32_t size;

  bool contains(uint32_t rva, uint32_t length) const {
    return rva <= size && length <= size - rva;
  }
  uint32_t word(uint32_t rva) { return as.get32(base + rva); }

  enum Kind : uint8_t { Nop, Alloc, Frame, Int, Float, LRPair, Chain, Next };
  struct Op {
    uint32_t offset;
    Kind kind;
    uint8_t reg;
    uint8_t count;
    bool writeback;
  };
  struct Program {
    // An extended xdata record contains at most 255 words of unwind codes.
    Op ops[1020];
    unsigned count = 0;
    unsigned length = 0; // instructions before end/end_c (excludes ret)
    bool append(Kind k, unsigned r = 0, unsigned n = 0, unsigned off = 0,
                bool wb = false) {
      if (count == 1020)
        return false;
      ops[count++] = {off, k, uint8_t(r), uint8_t(n), wb};
      return true;
    }
  };

  bool decode(uint32_t codes, unsigned bytes, unsigned index, Program &p) {
    unsigned next = 0;
    bool chained = false;
    while (index < bytes) {
      unsigned b = as.get8(base + codes + index++), v = 0;
      unsigned len = b < 0xc0    ? 1
                     : b < 0xe0  ? 2
                     : b == 0xe0 ? 4
                     : b == 0xe2 ? 2
                     : b == 0xe7 ? 3
                                 : 1;
      if (len - 1 > bytes - index)
        return false;
      for (unsigned i = 1; i < len; ++i)
        v = (v << 8) | as.get8(base + codes + index++);
      if (b == 0xe4)
        return next == 0;
      if (b == 0xe5) {
        if (next || !p.append(Chain))
          return false;
        chained = true;
        continue;
      }
      if (!chained)
        ++p.length;
      Op o = {0, Nop, 0, 0, false};
      if (b < 0x20)
        o = {b * 16, Alloc, 0, 0, false};
      else if (b < 0x40)
        o = {(b & 31) * 8, Int, 19, 2, true};
      else if (b < 0x80)
        o = {(b & 63) * 8, Int, 29, 2, false};
      else if (b < 0xc0)
        o = {((b & 63) + 1) * 8, Int, 29, 2, true};
      else if (b < 0xc8)
        o = {(((b & 7) << 8) | v) * 16, Alloc, 0, 0, false};
      else if (b < 0xd4)
        o = {((v & 63) + (b >= 0xcc && b < 0xd0)) * 8, Int,
             uint8_t(19 + ((b & 3) << 2) + (v >> 6)), uint8_t(b < 0xd0 ? 2 : 1),
             b >= 0xcc && b < 0xd0};
      else if (b < 0xd6)
        o = {((v & 31) + 1) * 8, Int, uint8_t(19 + ((b & 1) << 3) + (v >> 5)),
             1, true};
      else if (b < 0xd8)
        o = {(v & 63) * 8, LRPair,
             uint8_t(19 + 2 * (((b & 1) << 2) + (v >> 6))), 2, false};
      else if (b < 0xde)
        o = {((v & 63) + (b >= 0xda && b < 0xdc)) * 8, Float,
             uint8_t(8 + ((b & 1) << 2) + (v >> 6)), uint8_t(b < 0xdc ? 2 : 1),
             b >= 0xda && b < 0xdc};
      else if (b == 0xde)
        o = {((v & 31) + 1) * 8, Float, uint8_t(8 + (v >> 5)), 1, true};
      else if (b == 0xe0)
        o = {v * 16, Alloc, 0, 0, false};
      else if (b == 0xe1 || b == 0xe2)
        o = {v * 8, Frame, 0, 0, false};
      else if (b == 0xe3) {
      } else if (b == 0xe6) {
        ++next;
        if (!p.append(Next))
          return false;
        continue;
      } else if (b == 0xe7) {
        unsigned r = v >> 8, off = v & 63, type = (v >> 6) & 3;
        if ((r & 128) || type >= 2)
          return false;
        bool pair = r & 64, wb = r & 32;
        o = {(off + wb) * (pair || wb ? 16u : 8u), type ? Float : Int,
             uint8_t(r & 31), uint8_t(pair ? 2 : 1), wb};
      } else
        return false;
      if ((o.kind == Int || o.kind == LRPair) && o.reg + o.count > 31)
        return false;
      if (o.kind == Float && o.reg + o.count > 32)
        return false;
      if (next) {
        if ((o.kind != Int && o.kind != Float) || o.count != 2 ||
            o.reg + 2 * next + 2 > (o.kind == Int ? 31u : 32u))
          return false;
        for (unsigned i = 1; i <= next; ++i)
          p.ops[p.count - i] = {(o.writeback ? 0 : o.offset) + 16 * i, o.kind,
                                uint8_t(o.reg + 2 * i), 2, false};
        next = 0;
      }
      if (!p.append(o.kind, o.reg, o.count, o.offset, o.writeback))
        return false;
    }
    return false; // no end opcode
  }

  // Construct the canonical prologue in execution order, then reverse it.
  bool packed(uint32_t data, Program &p, bool epilog) {
    unsigned ri = (data >> 16) & 15, rf = (data >> 13) & 7;
    unsigned cr = (data >> 21) & 3, h = (data >> 20) & 1;
    unsigned frame = (data >> 23) * 16;
    rf = rf ? rf + 1 : 0;
    unsigned ints = ri * 8 + (cr == 1 ? 8 : 0);
    unsigned save = (ints + rf * 8 + h * 64 + 15) & ~15u;
    if (ri > 10 || cr == 2 || save > frame)
      return false;
    unsigned local = frame - save;
    bool allocated = false;
    for (unsigned r = 0; r < ri;) {
      unsigned n = ri - r >= 2 ? 2 : 1;
      bool lr = n == 1 && cr == 1;
      p.append(lr ? LRPair : Int, 19 + r, lr ? 2 : n, allocated ? r * 8 : save,
               !allocated);
      allocated = true;
      r += n;
    }
    if (cr == 1 && !(ri & 1)) {
      p.append(Int, 30, 1, allocated ? ri * 8 : save, !allocated);
      allocated = true;
    }
    for (unsigned r = 0; r < rf; r += 2) {
      p.append(Float, 8 + r, rf - r >= 2 ? 2 : 1,
               allocated ? ints + r * 8 : save, !allocated);
      allocated = true;
    }
    // Homing alone uses an explicit allocation, not a writeback on x0/x1.
    if (save && !allocated)
      p.append(Alloc, 0, 0, save);
    if (h && !epilog)
      for (unsigned i = 0; i < 4; ++i)
        p.append(Nop);
    if (cr == 3 && local < 16)
      return false;
    if (cr == 3 && local <= 512) {
      p.append(Int, 29, 2, local, true);
    } else {
      if (local > 4080) {
        p.append(Alloc, 0, 0, 4080);
        local -= 4080;
      }
      if (local)
        p.append(Alloc, 0, 0, local);
      if (cr == 3)
        p.append(Int, 29, 2);
    }
    // Packed frames never restore SP from FP, even for a body unwind.
    if (cr == 3 && !epilog)
      p.append(Nop);
    p.length = p.count;
    for (unsigned i = 0; i < p.count / 2; ++i) {
      Op tmp = p.ops[i];
      p.ops[i] = p.ops[p.count - i - 1];
      p.ops[p.count - i - 1] = tmp;
    }
    return true;
  }

public:
  struct Info {
    uint32_t begin = 0, end = 0, entry = 0, data = 0;
    uint32_t codes = 0, bytes = 0, scopes = 0, epilogs = 0;
    uint32_t handler = 0, lsda = 0;
    bool single = false;
  };

  UnwindPEArm64(A &a, pint_t imageBase, uint32_t imageSize)
      : as(a), base(imageBase), size(imageSize) {}

  bool read(uint32_t entry, Info &i) {
    i = Info();
    if (!contains(entry, 8))
      return false;
    i.entry = entry;
    i.begin = word(entry);
    i.data = word(entry + 4);
    unsigned length;
    if (i.data & 3) {
      if ((i.data & 3) == 3)
        return false;
      length = ((i.data >> 2) & 2047) * 4;
    } else {
      uint32_t x = i.data;
      if (!contains(x, 4))
        return false;
      uint32_t header = word(x);
      if (header & (3 << 18))
        return false;
      length = (header & 0x3ffff) * 4;
      i.single = header & (1 << 21);
      i.epilogs = (header >> 22) & 31;
      i.bytes = (header >> 27) * 4;
      x += 4;
      if (!i.epilogs && !i.bytes) {
        if (!contains(x, 4))
          return false;
        uint32_t ext = word(x);
        if (ext >> 24)
          return false;
        i.epilogs = ext & 0xffff;
        i.bytes = ((ext >> 16) & 255) * 4;
        x += 4;
      }
      i.scopes = x;
      unsigned scopeBytes = i.single ? 0 : i.epilogs * 4;
      if (!contains(x, scopeBytes + i.bytes + ((header & (1 << 20)) ? 4 : 0)))
        return false;
      i.codes = x + scopeBytes;
      if (!i.bytes || (i.single && i.epilogs >= i.bytes))
        return false;
      if (header & (1 << 20)) {
        i.handler = word(i.codes + i.bytes);
        i.lsda = i.codes + i.bytes + 4;
        if (!i.handler || !contains(i.handler, 4) || !contains(i.lsda, 1))
          return false;
      }
    }
    if (!length || !contains(i.begin, length) || (i.begin & 3))
      return false;
    i.end = i.begin + length;
    return true;
  }

  // 1: table entry, 0: leaf in a known executable section, -1: bad image/table.
  int find(uint32_t pc, Info &i) {
    if (!contains(0, 64) || as.get16(base) != 0x5a4d)
      return -1;
    uint32_t nt = word(0x3c);
    if (!contains(nt, 24 + 112) || word(nt) != 0x4550 ||
        as.get16(base + nt + 4) != 0xaa64 || as.get16(base + nt + 24) != 0x20b)
      return -1;
    unsigned opt = as.get16(base + nt + 20);
    unsigned sections = as.get16(base + nt + 6);
    if (opt < 112 || !contains(nt + 24, opt + sections * 40))
      return -1;
    bool executable = false;
    for (unsigned s = nt + 24 + opt; sections; --sections, s += 40) {
      uint32_t start = word(s + 12), length = word(s + 8);
      if (contains(start, length) && pc >= start && pc - start < length &&
          (word(s + 36) & 0x20000000))
        executable = true;
    }
    if (!executable)
      return -1;
    if (word(nt + 24 + 108) < 4)
      return 0; // no exception directory: executable leaves only
    if (opt < 144)
      return -1;
    uint32_t table = word(nt + 24 + 136), bytes = word(nt + 24 + 140);
    if ((table & 3) || (bytes & 7) || !contains(table, bytes))
      return -1;
    unsigned lo = 0, hi = bytes / 8;
    while (lo < hi) {
      unsigned mid = lo + (hi - lo) / 2;
      if (word(table + mid * 8) <= pc)
        lo = mid + 1;
      else
        hi = mid;
    }
    if (!lo)
      return 0;
    if (!read(table + (lo - 1) * 8, i))
      return -1;
    return pc < i.end ? 1 : 0;
  }

  template <class R> bool step(const Info &i, uint32_t pc, R &registers) {
    Program p;
    unsigned offset = (pc - i.begin) / 4, skip = 0;
    if (pc < i.begin || pc >= i.end)
      return false;
    if (i.data & 3) {
      if (!packed(i.data, p, false) ||
          ((i.data & 3) == 1 && p.length > (i.end - i.begin) / 4))
        return false;
      if ((i.data & 3) == 1) {
        if (offset < p.length)
          skip = p.length - offset;
        else {
          p.count = p.length = 0;
          if (!packed(i.data, p, true))
            return false;
          unsigned length = (i.end - i.begin) / 4;
          if (p.length + 1 > length)
            return false;
          unsigned start = length - p.length - 1;
          if (offset >= start)
            skip = offset - start;
        }
      }
    } else {
      if (!decode(i.codes, i.bytes, 0, p) || p.length > (i.end - i.begin) / 4)
        return false;
      if (offset < p.length)
        skip = p.length - offset;
      else if (i.single) {
        p.count = p.length = 0;
        if (!decode(i.codes, i.bytes, i.epilogs, p))
          return false;
        unsigned length = (i.end - i.begin) / 4;
        if (p.length + 1 > length)
          return false;
        unsigned start = length - p.length - 1;
        if (offset >= start)
          skip = offset - start;
        else {
          p.count = p.length = 0;
          if (!decode(i.codes, i.bytes, 0, p))
            return false;
        }
      } else {
        for (unsigned e = 0; e < i.epilogs; ++e) {
          uint32_t scope = word(i.scopes + e * 4);
          unsigned start = scope & 0x3ffff;
          if ((scope & 0x003c0000) || start >= (i.end - i.begin) / 4)
            return false;
          if (offset < start)
            break;
          p.count = p.length = 0;
          if (!decode(i.codes, i.bytes, scope >> 22, p))
            return false;
          if (offset - start <= p.length) {
            skip = offset - start;
            break;
          }
          p.count = p.length = 0;
          if (!decode(i.codes, i.bytes, 0, p))
            return false;
        }
      }
    }
    // Work on a copy: invalid metadata must not leave a half-updated cursor.
    R next = registers;
    for (unsigned n = 0; n < p.count; ++n) {
      Op o = p.ops[n];
      if (o.kind == Chain)
        continue;
      if (skip) {
        --skip;
        continue;
      }
      uint64_t sp = next.getSP();
      if (o.kind == Alloc)
        next.setSP(sp + o.offset);
      else if (o.kind == Frame)
        next.setSP(next.getFP() - o.offset);
      else if (o.kind == Int || o.kind == Float || o.kind == LRPair) {
        uint64_t addr = sp + (o.writeback ? 0 : o.offset);
        for (unsigned r = 0; r < o.count; ++r) {
          if (o.kind == Float)
            next.setFloatRegister(64 + o.reg + r, as.getDouble(addr + r * 8));
          else
            next.setRegister(o.kind == LRPair && r ? 30 : o.reg + r,
                             as.get64(addr + r * 8));
        }
        if (o.writeback)
          next.setSP(sp + o.offset);
      }
    }
    if (skip || (next.getSP() & 15))
      return false;
    next.setIP(next.getRegister(30));
    if (next.getIP() == registers.getIP() && next.getSP() == registers.getSP())
      return false;
    registers = next;
    return true;
  }
};

} // namespace libunwind
#endif
