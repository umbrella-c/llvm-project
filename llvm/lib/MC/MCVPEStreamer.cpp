//===- llvm/MC/MCVPEStreamer.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains an implementation of a Windows COFF object file streamer.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/COFF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCFragment.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectStreamer.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSymbolVPE.h"
#include "llvm/MC/MCVPEStreamer.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <cstdint>

using namespace llvm;

#define DEBUG_TYPE "VPEStreamer"

MCVPEStreamer::MCVPEStreamer(MCContext &Context,
                                     std::unique_ptr<MCAsmBackend> MAB,
                                     std::unique_ptr<MCCodeEmitter> CE,
                                     std::unique_ptr<MCObjectWriter> OW)
    : MCObjectStreamer(Context, std::move(MAB), std::move(OW), std::move(CE)),
      CurSymbol(nullptr) {}

void MCVPEStreamer::emitInstToData(const MCInst &Inst,
                                       const MCSubtargetInfo &STI) {
  MCDataFragment *DF = getOrCreateDataFragment();

  SmallVector<MCFixup, 4> Fixups;
  SmallString<256> Code;
  raw_svector_ostream VecOS(Code);
  getAssembler().getEmitter().encodeInstruction(Inst, VecOS, Fixups, STI);

  // Add the fixups and data.
  for (unsigned i = 0, e = Fixups.size(); i != e; ++i) {
    Fixups[i].setOffset(Fixups[i].getOffset() + DF->getContents().size());
    DF->getFixups().push_back(Fixups[i]);
  }
  DF->setHasInstructions(STI);
  DF->getContents().append(Code.begin(), Code.end());
}

void MCVPEStreamer::InitSections(bool NoExecStack) {
  // FIXME: this is identical to the ELF one.
  // This emulates the same behavior of GNU as. This makes it easier
  // to compare the output as the major sections are in the same order.
  SwitchSection(getContext().getObjectFileInfo()->getTextSection());
  emitCodeAlignment(4);

  SwitchSection(getContext().getObjectFileInfo()->getDataSection());
  emitCodeAlignment(4);

  SwitchSection(getContext().getObjectFileInfo()->getBSSSection());
  emitCodeAlignment(4);

  SwitchSection(getContext().getObjectFileInfo()->getTextSection());
}

void MCVPEStreamer::emitLabel(MCSymbol *S, SMLoc Loc) {
  auto *Symbol = cast<MCSymbolVPE>(S);
  MCObjectStreamer::emitLabel(Symbol, Loc);
}

void MCVPEStreamer::emitAssemblerFlag(MCAssemblerFlag Flag) {
  // Let the target do whatever target specific stuff it needs to do.
  getAssembler().getBackend().handleAssemblerFlag(Flag);

  switch (Flag) {
  // None of these require COFF specific handling.
  case MCAF_SyntaxUnified:
  case MCAF_Code16:
  case MCAF_Code32:
  case MCAF_Code64:
    break;
  case MCAF_SubsectionsViaSymbols:
    llvm_unreachable("VPE doesn't support .subsections_via_symbols");
  }
}

void MCVPEStreamer::emitThumbFunc(MCSymbol *Func) {
  llvm_unreachable("not implemented");
}

bool MCVPEStreamer::emitSymbolAttribute(MCSymbol *S,
                                        MCSymbolAttr Attribute) {
  auto *Symbol = cast<MCSymbolVPE>(S);
  getAssembler().registerSymbol(*Symbol);

  switch (Attribute) {
  default: return false;
  case MCSA_WeakReference:
  case MCSA_Weak:
    Symbol->setIsWeakExternal();
    Symbol->setExternal(true);
    break;
  case MCSA_Global:
    Symbol->setExternal(true);
    break;
  case MCSA_AltEntry:
    llvm_unreachable("COFF doesn't support the .alt_entry attribute");
  }

  return true;
}

void MCVPEStreamer::emitSymbolDesc(MCSymbol *Symbol, unsigned DescValue) {
  llvm_unreachable("not implemented");
}

void MCVPEStreamer::BeginCOFFSymbolDef(MCSymbol const *S) {
  auto *Symbol = cast<MCSymbolVPE>(S);
  if (CurSymbol)
    Error("starting a new symbol definition without completing the "
          "previous one");
  CurSymbol = Symbol;
}

void MCVPEStreamer::EmitCOFFSymbolStorageClass(int StorageClass) {
  if (!CurSymbol) {
    Error("storage class specified outside of symbol definition");
    return;
  }

  if (StorageClass & ~COFF::SSC_Invalid) {
    Error("storage class value '" + Twine(StorageClass) +
               "' out of range");
    return;
  }

  getAssembler().registerSymbol(*CurSymbol);
  cast<MCSymbolVPE>(CurSymbol)->setClass((uint16_t)StorageClass);
}

void MCVPEStreamer::EmitCOFFSymbolType(int Type) {
  if (!CurSymbol) {
    Error("symbol type specified outside of a symbol definition");
    return;
  }

  if (Type & ~0xffff) {
    Error("type value '" + Twine(Type) + "' out of range");
    return;
  }

  getAssembler().registerSymbol(*CurSymbol);
  cast<MCSymbolVPE>(CurSymbol)->setType((uint16_t)Type);
}

void MCVPEStreamer::EndCOFFSymbolDef() {
  if (!CurSymbol)
    Error("ending symbol definition without starting one");
  CurSymbol = nullptr;
}

void MCVPEStreamer::EmitCOFFSymbolIndex(MCSymbol const *Symbol) {
  MCSection *Sec = getCurrentSectionOnly();
  getAssembler().registerSection(*Sec);
  if (Sec->getAlignment() < 4)
    Sec->setAlignment(Align(4));

  new MCSymbolIdFragment(Symbol, getCurrentSectionOnly());

  getAssembler().registerSymbol(*Symbol);
}

void MCVPEStreamer::EmitCOFFSectionIndex(const MCSymbol *Symbol) {
  visitUsedSymbol(*Symbol);
  MCDataFragment *DF = getOrCreateDataFragment();
  const MCSymbolRefExpr *SRE = MCSymbolRefExpr::create(Symbol, getContext());
  MCFixup Fixup = MCFixup::create(DF->getContents().size(), SRE, FK_SecRel_2);
  DF->getFixups().push_back(Fixup);
  DF->getContents().resize(DF->getContents().size() + 2, 0);
}

void MCVPEStreamer::EmitCOFFSecRel32(const MCSymbol *Symbol,
                                         uint64_t Offset) {
  visitUsedSymbol(*Symbol);
  MCDataFragment *DF = getOrCreateDataFragment();
  // Create Symbol A for the relocation relative reference.
  const MCExpr *MCE = MCSymbolRefExpr::create(Symbol, getContext());
  // Add the constant offset, if given.
  if (Offset)
    MCE = MCBinaryExpr::createAdd(
        MCE, MCConstantExpr::create(Offset, getContext()), getContext());
  // Build the secrel32 relocation.
  MCFixup Fixup = MCFixup::create(DF->getContents().size(), MCE, FK_SecRel_4);
  // Record the relocation.
  DF->getFixups().push_back(Fixup);
  // Emit 4 bytes (zeros) to the object file.
  DF->getContents().resize(DF->getContents().size() + 4, 0);
}

void MCVPEStreamer::EmitCOFFImgRel32(const MCSymbol *Symbol,
                                         int64_t Offset) {
  visitUsedSymbol(*Symbol);
  MCDataFragment *DF = getOrCreateDataFragment();
  // Create Symbol A for the relocation relative reference.
  const MCExpr *MCE = MCSymbolRefExpr::create(
      Symbol, MCSymbolRefExpr::VK_COFF_IMGREL32, getContext());
  // Add the constant offset, if given.
  if (Offset)
    MCE = MCBinaryExpr::createAdd(
        MCE, MCConstantExpr::create(Offset, getContext()), getContext());
  // Build the imgrel relocation.
  MCFixup Fixup = MCFixup::create(DF->getContents().size(), MCE, FK_Data_4);
  // Record the relocation.
  DF->getFixups().push_back(Fixup);
  // Emit 4 bytes (zeros) to the object file.
  DF->getContents().resize(DF->getContents().size() + 4, 0);
}

void MCVPEStreamer::emitCommonSymbol(MCSymbol *S, uint64_t Size,
                                     unsigned ByteAlignment) {
  auto *Symbol = cast<MCSymbolVPE>(S);

  getAssembler().registerSymbol(*Symbol);
  Symbol->setExternal(true);
  Symbol->setCommon(Size, ByteAlignment);

  if (ByteAlignment > 1) {
    SmallString<128> Directive;
    raw_svector_ostream OS(Directive);
    const MCObjectFileInfo *MFI = getContext().getObjectFileInfo();

    OS << " -aligncomm:\"" << Symbol->getName() << "\","
       << Log2_32_Ceil(ByteAlignment);

    PushSection();
    SwitchSection(MFI->getDrectveSection());
    emitBytes(Directive);
    PopSection();
  }
}

void MCVPEStreamer::emitLocalCommonSymbol(MCSymbol *S, uint64_t Size,
                                              unsigned ByteAlignment) {
  auto *Symbol = cast<MCSymbolVPE>(S);

  MCSection *Section = getContext().getObjectFileInfo()->getBSSSection();
  PushSection();
  SwitchSection(Section);
  emitValueToAlignment(ByteAlignment, 0, 1, 0);
  emitLabel(Symbol);
  Symbol->setExternal(false);
  emitZeros(Size);
  PopSection();
}

void MCVPEStreamer::emitWeakReference(MCSymbol *AliasS,
                                      const MCSymbol *Symbol) {
  auto *Alias = cast<MCSymbolVPE>(AliasS);
  emitSymbolAttribute(Alias, MCSA_Weak);

  getAssembler().registerSymbol(*Symbol);
  Alias->setVariableValue(MCSymbolRefExpr::create(
      Symbol, MCSymbolRefExpr::VK_WEAKREF, getContext()));
}

void MCVPEStreamer::emitZerofill(MCSection *Section, MCSymbol *Symbol,
                                 uint64_t Size, unsigned ByteAlignment,
                                 SMLoc Loc) {
  llvm_unreachable("not implemented");
}

void MCVPEStreamer::emitTBSSSymbol(MCSection *Section, MCSymbol *Symbol,
                                   uint64_t Size, unsigned ByteAlignment) {
  llvm_unreachable("not implemented");
}

// TODO: Implement this if you want to emit .comment section in VPE obj files.
void MCVPEStreamer::emitIdent(StringRef IdentString) {
  llvm_unreachable("not implemented");
}

void MCVPEStreamer::emitCGProfileEntry(const MCSymbolRefExpr *From,
                                       const MCSymbolRefExpr *To,
                                       uint64_t Count) {
  // Ignore temporary symbols for now.
  if (!From->getSymbol().isTemporary() && !To->getSymbol().isTemporary())
    getAssembler().CGProfile.push_back({From, To, Count});
}

void MCVPEStreamer::finalizeCGProfileEntry(const MCSymbolRefExpr *&SRE) {
  const MCSymbol *S = &SRE->getSymbol();
  bool Created;
  getAssembler().registerSymbol(*S, &Created);
  if (Created)
    cast<MCSymbolVPE>(S)->setExternal(true);
}

void MCVPEStreamer::finalizeCGProfile() {
  for (MCAssembler::CGProfileEntry &E : getAssembler().CGProfile) {
    finalizeCGProfileEntry(E.From);
    finalizeCGProfileEntry(E.To);
  }
}

void MCVPEStreamer::finishImpl() {
  finalizeCGProfile();
  MCObjectStreamer::finishImpl();
}

void MCVPEStreamer::Error(const Twine &Msg) const {
  getContext().reportError(SMLoc(), Msg);
}
