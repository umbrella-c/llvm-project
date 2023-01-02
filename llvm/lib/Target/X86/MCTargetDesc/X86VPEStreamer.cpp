//===-- X86WinCOFFStreamer.cpp - X86 Target WinCOFF Streamer ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "X86MCTargetDesc.h"
#include "X86TargetStreamer.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCVPEStreamer.h"

using namespace llvm;

MCStreamer *llvm::createX86VPEStreamer(const Triple &T, MCContext &C,
                                       std::unique_ptr<MCAsmBackend> &&AB,
                                       std::unique_ptr<MCObjectWriter> &&OW,
                                       std::unique_ptr<MCCodeEmitter> &&CE,
                                       bool RelaxAll) {
  MCVPEStreamer *S =
      new MCVPEStreamer(C, std::move(AB), std::move(CE), std::move(OW));
  S->getAssembler().setRelaxAll(RelaxAll);
  return S;
}
