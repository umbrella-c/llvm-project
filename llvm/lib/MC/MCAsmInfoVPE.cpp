//===- MCAsmInfoCOFF.cpp - COFF asm properties ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines target asm properties related what form asm statements
// should take in general on COFF-based targets
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCAsmInfoVPE.h"
#include "llvm/MC/MCDirectives.h"

using namespace llvm;

void MCAsmInfoVPE::anchor() {}

MCAsmInfoVPE::MCAsmInfoVPE() {
  // MingW 4.5 and later support .comm with log2 alignment, but .lcomm uses byte
  // alignment.
  COMMDirectiveAlignmentIsInBytes = false;
  LCOMMDirectiveAlignmentType = LCOMM::ByteAlignment;
  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = true;
  WeakRefDirective = "\t.weak\t";
  AvoidWeakIfComdat = true;

  // Doesn't support visibility:
  HiddenVisibilityAttr = HiddenDeclarationVisibilityAttr = MCSA_Invalid;
  ProtectedVisibilityAttr = MCSA_Invalid;

  // Set up DWARF directives
  SupportsDebugInformation = true;
  NeedsDwarfSectionOffsetDirective = true;

  // At least MSVC inline-asm does AShr.
  UseLogicalShr = false;

  // If this is a COFF target, assume that it supports associative comdats. It's
  // part of the spec.
  HasCOFFAssociativeComdats = true;

  // We can generate constants in comdat sections that can be shared,
  // but in order not to create null typed symbols, we actually need to
  // make them global symbols as well.
  HasCOFFComdatConstants = true;
}
