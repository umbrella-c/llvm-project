//===- lib/MC/MCSectionCOFF.cpp - COFF Code Section Representation --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCSectionVPE.h"
#include "llvm/BinaryFormat/VPE.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>

using namespace llvm;

// ShouldOmitSectionDirective - Decides whether a '.section' directive
// should be printed before the section name
bool MCSectionVPE::shouldOmitSectionDirective(StringRef Name,
                                               const MCAsmInfo &MAI) const {
  if (COMDATSymbol)
    return false;
  if (Name == ".text" || Name == ".data" || Name == ".bss")
    return true;
  return false;
}

void MCSectionVPE::setSelection(int Selection) const {
  assert(Selection != 0 && "invalid COMDAT selection type");
  this->Selection = Selection;
  Characteristics |= VPE::IMAGE_SCN_LNK_COMDAT;
}

void MCSectionVPE::printSwitchToSection(const MCAsmInfo &MAI, const Triple &T,
                                         raw_ostream &OS,
                                         const MCExpr *Subsection) const {
  // standard sections don't require the '.section'
  if (shouldOmitSectionDirective(getName(), MAI)) {
    OS << '\t' << getName() << '\n';
    return;
  }

  OS << "\t.section\t" << getName() << ",\"";
  if (getCharacteristics() & VPE::IMAGE_SCN_CNT_INITIALIZED_DATA)
    OS << 'd';
  if (getCharacteristics() & VPE::IMAGE_SCN_CNT_UNINITIALIZED_DATA)
    OS << 'b';
  if (getCharacteristics() & VPE::IMAGE_SCN_MEM_EXECUTE)
    OS << 'x';
  if (getCharacteristics() & VPE::IMAGE_SCN_MEM_WRITE)
    OS << 'w';
  else if (getCharacteristics() & VPE::IMAGE_SCN_MEM_READ)
    OS << 'r';
  else
    OS << 'y';
  if (getCharacteristics() & VPE::IMAGE_SCN_LNK_REMOVE)
    OS << 'n';
  if (getCharacteristics() & VPE::IMAGE_SCN_MEM_SHARED)
    OS << 's';
  if ((getCharacteristics() & VPE::IMAGE_SCN_MEM_DISCARDABLE) &&
      !isImplicitlyDiscardable(getName()))
    OS << 'D';
  OS << '"';

  if (getCharacteristics() & VPE::IMAGE_SCN_LNK_COMDAT) {
    if (COMDATSymbol)
      OS << ",";
    else
      OS << "\n\t.linkonce\t";
    switch (Selection) {
      case VPE::IMAGE_COMDAT_SELECT_NODUPLICATES:
        OS << "one_only";
        break;
      case VPE::IMAGE_COMDAT_SELECT_ANY:
        OS << "discard";
        break;
      case VPE::IMAGE_COMDAT_SELECT_SAME_SIZE:
        OS << "same_size";
        break;
      case VPE::IMAGE_COMDAT_SELECT_EXACT_MATCH:
        OS << "same_contents";
        break;
      case VPE::IMAGE_COMDAT_SELECT_ASSOCIATIVE:
        OS << "associative";
        break;
      case VPE::IMAGE_COMDAT_SELECT_LARGEST:
        OS << "largest";
        break;
      case VPE::IMAGE_COMDAT_SELECT_NEWEST:
        OS << "newest";
        break;
      default:
        assert(false && "unsupported COFF selection type");
        break;
    }
    if (COMDATSymbol) {
      OS << ",";
      COMDATSymbol->print(OS, &MAI);
    }
  }
  OS << '\n';
}

bool MCSectionVPE::useCodeAlign() const {
  return getKind().isText();
}

bool MCSectionVPE::isVirtualSection() const {
  return getCharacteristics() & VPE::IMAGE_SCN_CNT_UNINITIALIZED_DATA;
}

StringRef MCSectionVPE::getVirtualSectionKind() const {
  return "IMAGE_SCN_CNT_UNINITIALIZED_DATA";
}
