//===- VPEAsmParser.cpp - VPE Assembly Parser -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/VPE.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParserExtension.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionVPE.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/SectionKind.h"
#include "llvm/Support/SMLoc.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>

using namespace llvm;

namespace {

class VPEAsmParser : public MCAsmParserExtension {
  template<bool (VPEAsmParser::*HandlerMethod)(StringRef, SMLoc)>
  void addDirectiveHandler(StringRef Directive) {
    MCAsmParser::ExtensionDirectiveHandler Handler = std::make_pair(
        this, HandleDirective<VPEAsmParser, HandlerMethod>);
    getParser().addDirectiveHandler(Directive, Handler);
  }

  bool ParseSectionSwitch(StringRef Section,
                          unsigned Characteristics,
                          SectionKind Kind);

  bool ParseSectionSwitch(StringRef Section, unsigned Characteristics,
                          SectionKind Kind, StringRef COMDATSymName,
                          VPE::COMDATType Type);

  bool ParseSectionName(StringRef &SectionName);
  bool ParseSectionFlags(StringRef SectionName, StringRef FlagsString,
                         unsigned *Flags);

  void Initialize(MCAsmParser &Parser) override {
    // Call the base implementation.
    MCAsmParserExtension::Initialize(Parser);

    addDirectiveHandler<&VPEAsmParser::ParseSectionDirectiveText>(".text");
    addDirectiveHandler<&VPEAsmParser::ParseSectionDirectiveData>(".data");
    addDirectiveHandler<&VPEAsmParser::ParseSectionDirectiveBSS>(".bss");
    addDirectiveHandler<&VPEAsmParser::ParseSectionDirectiveEhFrame>(".eh_frame");

    addDirectiveHandler<&VPEAsmParser::ParseDirectiveSection>(".section");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveDef>(".def");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveScl>(".scl");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveType>(".type");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveEndef>(".endef");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveSecRel32>(".secrel32");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveSymIdx>(".symidx");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveSecIdx>(".secidx");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveLinkOnce>(".linkonce");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveRVA>(".rva");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveSymbolAttribute>(".weak");
    addDirectiveHandler<&VPEAsmParser::ParseDirectiveCGProfile>(".cg_profile");
  }

  bool ParseSectionDirectiveText(StringRef, SMLoc) {
    return ParseSectionSwitch(".text",
                              VPE::IMAGE_SCN_CNT_CODE
                            | VPE::IMAGE_SCN_MEM_EXECUTE
                            | VPE::IMAGE_SCN_MEM_READ,
                              SectionKind::getText());
  }

  bool ParseSectionDirectiveData(StringRef, SMLoc) {
    return ParseSectionSwitch(".data", VPE::IMAGE_SCN_CNT_INITIALIZED_DATA |
                                           VPE::IMAGE_SCN_MEM_READ |
                                           VPE::IMAGE_SCN_MEM_WRITE,
                              SectionKind::getData());
  }

  bool ParseSectionDirectiveBSS(StringRef, SMLoc) {
    return ParseSectionSwitch(".bss",
                              VPE::IMAGE_SCN_CNT_UNINITIALIZED_DATA
                            | VPE::IMAGE_SCN_MEM_READ
                            | VPE::IMAGE_SCN_MEM_WRITE,
                              SectionKind::getBSS());
  }
  bool ParseSectionDirectiveEhFrame(StringRef, SMLoc) {
    return ParseSectionSwitch(".eh_frame",
                              VPE::IMAGE_SCN_CNT_INITIALIZED_DATA
                            | VPE::IMAGE_SCN_MEM_READ
                            | VPE::IMAGE_SCN_MEM_WRITE,
                              SectionKind::getData());
  }
  bool ParseDirectiveSection(StringRef, SMLoc);
  bool ParseDirectiveDef(StringRef, SMLoc);
  bool ParseDirectiveScl(StringRef, SMLoc);
  bool ParseDirectiveType(StringRef, SMLoc);
  bool ParseDirectiveEndef(StringRef, SMLoc);
  bool ParseDirectiveSecRel32(StringRef, SMLoc);
  bool ParseDirectiveSecIdx(StringRef, SMLoc);
  bool ParseDirectiveSymIdx(StringRef, SMLoc);
  bool parseCOMDATType(VPE::COMDATType &Type);
  bool ParseDirectiveLinkOnce(StringRef, SMLoc);
  bool ParseDirectiveSymbolAttribute(StringRef Directive, SMLoc);
  bool ParseDirectiveRVA(StringRef, SMLoc);
  bool ParseDirectiveCGProfile(StringRef, SMLoc);

public:
  VPEAsmParser() = default;
};

} // end annonomous namespace.

static SectionKind computeSectionKind(unsigned Flags) {
  if (Flags & VPE::IMAGE_SCN_MEM_EXECUTE)
    return SectionKind::getText();
  if (Flags & VPE::IMAGE_SCN_MEM_READ &&
      (Flags & VPE::IMAGE_SCN_MEM_WRITE) == 0)
    return SectionKind::getReadOnly();
  return SectionKind::getData();
}

bool VPEAsmParser::ParseSectionFlags(StringRef SectionName,
                                     StringRef FlagsString, unsigned *Flags) {
  enum {
    None        = 0,
    Alloc       = 1 << 0,
    Code        = 1 << 1,
    Load        = 1 << 2,
    InitData    = 1 << 3,
    Shared      = 1 << 4,
    NoLoad      = 1 << 5,
    NoRead      = 1 << 6,
    NoWrite     = 1 << 7,
    Discardable = 1 << 8,
  };

  bool ReadOnlyRemoved = false;
  unsigned SecFlags = None;

  for (char FlagChar : FlagsString) {
    switch (FlagChar) {
    case 'a':
      // Ignored.
      break;

    case 'b': // bss section
      SecFlags |= Alloc;
      if (SecFlags & InitData)
        return TokError("conflicting section flags 'b' and 'd'.");
      SecFlags &= ~Load;
      break;

    case 'd': // data section
      SecFlags |= InitData;
      if (SecFlags & Alloc)
        return TokError("conflicting section flags 'b' and 'd'.");
      SecFlags &= ~NoWrite;
      if ((SecFlags & NoLoad) == 0)
        SecFlags |= Load;
      break;

    case 'n': // section is not loaded
      SecFlags |= NoLoad;
      SecFlags &= ~Load;
      break;

    case 'D': // discardable
      SecFlags |= Discardable;
      break;

    case 'r': // read-only
      ReadOnlyRemoved = false;
      SecFlags |= NoWrite;
      if ((SecFlags & Code) == 0)
        SecFlags |= InitData;
      if ((SecFlags & NoLoad) == 0)
        SecFlags |= Load;
      break;

    case 's': // shared section
      SecFlags |= Shared | InitData;
      SecFlags &= ~NoWrite;
      if ((SecFlags & NoLoad) == 0)
        SecFlags |= Load;
      break;

    case 'w': // writable
      SecFlags &= ~NoWrite;
      ReadOnlyRemoved = true;
      break;

    case 'x': // executable section
      SecFlags |= Code;
      if ((SecFlags & NoLoad) == 0)
        SecFlags |= Load;
      if (!ReadOnlyRemoved)
        SecFlags |= NoWrite;
      break;

    case 'y': // not readable
      SecFlags |= NoRead | NoWrite;
      break;

    default:
      return TokError("unknown flag");
    }
  }

  *Flags = 0;

  if (SecFlags == None)
    SecFlags = InitData;

  if (SecFlags & Code)
    *Flags |= VPE::IMAGE_SCN_CNT_CODE | VPE::IMAGE_SCN_MEM_EXECUTE;
  if (SecFlags & InitData)
    *Flags |= VPE::IMAGE_SCN_CNT_INITIALIZED_DATA;
  if ((SecFlags & Alloc) && (SecFlags & Load) == 0)
    *Flags |= VPE::IMAGE_SCN_CNT_UNINITIALIZED_DATA;
  if (SecFlags & NoLoad)
    *Flags |= VPE::IMAGE_SCN_LNK_REMOVE;
  if ((SecFlags & Discardable) ||
      MCSectionVPE::isImplicitlyDiscardable(SectionName))
    *Flags |= VPE::IMAGE_SCN_MEM_DISCARDABLE;
  if ((SecFlags & NoRead) == 0)
    *Flags |= VPE::IMAGE_SCN_MEM_READ;
  if ((SecFlags & NoWrite) == 0)
    *Flags |= VPE::IMAGE_SCN_MEM_WRITE;
  if (SecFlags & Shared)
    *Flags |= VPE::IMAGE_SCN_MEM_SHARED;

  return false;
}

/// ParseDirectiveSymbolAttribute
///  ::= { ".weak", ... } [ identifier ( , identifier )* ]
bool VPEAsmParser::ParseDirectiveSymbolAttribute(StringRef Directive, SMLoc) {
  MCSymbolAttr Attr = StringSwitch<MCSymbolAttr>(Directive)
    .Case(".weak", MCSA_Weak)
    .Default(MCSA_Invalid);
  assert(Attr != MCSA_Invalid && "unexpected symbol attribute directive!");
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    while (true) {
      StringRef Name;

      if (getParser().parseIdentifier(Name))
        return TokError("expected identifier in directive");

      MCSymbol *Sym = getContext().getOrCreateSymbol(Name);

      getStreamer().emitSymbolAttribute(Sym, Attr);

      if (getLexer().is(AsmToken::EndOfStatement))
        break;

      if (getLexer().isNot(AsmToken::Comma))
        return TokError("unexpected token in directive");
      Lex();
    }
  }

  Lex();
  return false;
}

bool VPEAsmParser::ParseDirectiveCGProfile(StringRef S, SMLoc Loc) {
  return MCAsmParserExtension::ParseDirectiveCGProfile(S, Loc);
}

bool VPEAsmParser::ParseSectionSwitch(StringRef Section,
                                       unsigned Characteristics,
                                       SectionKind Kind) {
  return ParseSectionSwitch(Section, Characteristics, Kind, "", (VPE::COMDATType)0);
}

bool VPEAsmParser::ParseSectionSwitch(StringRef Section,
                                       unsigned Characteristics,
                                       SectionKind Kind,
                                       StringRef COMDATSymName,
                                       VPE::COMDATType Type) {
  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in section switching directive");
  Lex();

  getStreamer().switchSection(getContext().getVPESection(
      Section, Characteristics, Kind, COMDATSymName, Type));

  return false;
}

bool VPEAsmParser::ParseSectionName(StringRef &SectionName) {
  if (!getLexer().is(AsmToken::Identifier))
    return true;

  SectionName = getTok().getIdentifier();
  Lex();
  return false;
}

// .section name [, "flags"] [, identifier [ identifier ], identifier]
//
// Supported flags:
//   a: Ignored.
//   b: BSS section (uninitialized data)
//   d: data section (initialized data)
//   n: "noload" section (removed by linker)
//   D: Discardable section
//   r: Readable section
//   s: Shared section
//   w: Writable section
//   x: Executable section
//   y: Not-readable section (clears 'r')
//
// Subsections are not supported.
bool VPEAsmParser::ParseDirectiveSection(StringRef, SMLoc) {
  StringRef SectionName;

  if (ParseSectionName(SectionName))
    return TokError("expected identifier in directive");

  unsigned Flags = VPE::IMAGE_SCN_CNT_INITIALIZED_DATA |
                   VPE::IMAGE_SCN_MEM_READ |
                   VPE::IMAGE_SCN_MEM_WRITE;

  if (getLexer().is(AsmToken::Comma)) {
    Lex();

    if (getLexer().isNot(AsmToken::String))
      return TokError("expected string in directive");

    StringRef FlagsStr = getTok().getStringContents();
    Lex();

    if (ParseSectionFlags(SectionName, FlagsStr, &Flags))
      return true;
  }

  VPE::COMDATType Type = (VPE::COMDATType)0;
  StringRef COMDATSymName;
  if (getLexer().is(AsmToken::Comma)) {
    Type = VPE::IMAGE_COMDAT_SELECT_ANY;
    Lex();

    Flags |= VPE::IMAGE_SCN_LNK_COMDAT;

    if (!getLexer().is(AsmToken::Identifier))
      return TokError("expected comdat type such as 'discard' or 'largest' "
                      "after protection bits");

    if (parseCOMDATType(Type))
      return true;

    if (getLexer().isNot(AsmToken::Comma))
      return TokError("expected comma in directive");
    Lex();

    if (getParser().parseIdentifier(COMDATSymName))
      return TokError("expected identifier in directive");
  }

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  SectionKind Kind = computeSectionKind(Flags);
  if (Kind.isText()) {
    const Triple &T = getContext().getTargetTriple();
    if (T.getArch() == Triple::arm || T.getArch() == Triple::thumb)
      Flags |= VPE::IMAGE_SCN_MEM_16BIT;
  }
  ParseSectionSwitch(SectionName, Flags, Kind, COMDATSymName, Type);
  return false;
}

bool VPEAsmParser::ParseDirectiveDef(StringRef, SMLoc) {
  StringRef SymbolName;

  if (getParser().parseIdentifier(SymbolName))
    return TokError("expected identifier in directive");

  MCSymbol *Sym = getContext().getOrCreateSymbol(SymbolName);

  getStreamer().beginCOFFSymbolDef(Sym);

  Lex();
  return false;
}

bool VPEAsmParser::ParseDirectiveScl(StringRef, SMLoc) {
  int64_t SymbolStorageClass;
  if (getParser().parseAbsoluteExpression(SymbolStorageClass))
    return true;

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  Lex();
  getStreamer().emitCOFFSymbolStorageClass(SymbolStorageClass);
  return false;
}

bool VPEAsmParser::ParseDirectiveType(StringRef, SMLoc) {
  int64_t Type;
  if (getParser().parseAbsoluteExpression(Type))
    return true;

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  Lex();
  getStreamer().emitCOFFSymbolType(Type);
  return false;
}

bool VPEAsmParser::ParseDirectiveEndef(StringRef, SMLoc) {
  Lex();
  getStreamer().endCOFFSymbolDef();
  return false;
}

bool VPEAsmParser::ParseDirectiveSecRel32(StringRef, SMLoc) {
  StringRef SymbolID;
  if (getParser().parseIdentifier(SymbolID))
    return TokError("expected identifier in directive");

  int64_t Offset = 0;
  SMLoc OffsetLoc;
  if (getLexer().is(AsmToken::Plus)) {
    OffsetLoc = getLexer().getLoc();
    if (getParser().parseAbsoluteExpression(Offset))
      return true;
  }

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  if (Offset < 0 || Offset > std::numeric_limits<uint32_t>::max())
    return Error(
        OffsetLoc,
        "invalid '.secrel32' directive offset, can't be less "
        "than zero or greater than std::numeric_limits<uint32_t>::max()");

  MCSymbol *Symbol = getContext().getOrCreateSymbol(SymbolID);

  Lex();
  getStreamer().emitCOFFSecRel32(Symbol, Offset);
  return false;
}

bool VPEAsmParser::ParseDirectiveRVA(StringRef, SMLoc) {
  auto parseOp = [&]() -> bool {
    StringRef SymbolID;
    if (getParser().parseIdentifier(SymbolID))
      return TokError("expected identifier in directive");

    int64_t Offset = 0;
    SMLoc OffsetLoc;
    if (getLexer().is(AsmToken::Plus) || getLexer().is(AsmToken::Minus)) {
      OffsetLoc = getLexer().getLoc();
      if (getParser().parseAbsoluteExpression(Offset))
        return true;
    }

    if (Offset < std::numeric_limits<int32_t>::min() ||
        Offset > std::numeric_limits<int32_t>::max())
      return Error(OffsetLoc, "invalid '.rva' directive offset, can't be less "
                              "than -2147483648 or greater than "
                              "2147483647");

    MCSymbol *Symbol = getContext().getOrCreateSymbol(SymbolID);

    getStreamer().emitCOFFImgRel32(Symbol, Offset);
    return false;
  };

  if (getParser().parseMany(parseOp))
    return addErrorSuffix(" in directive");
  return false;
}

bool VPEAsmParser::ParseDirectiveSecIdx(StringRef, SMLoc) {
  StringRef SymbolID;
  if (getParser().parseIdentifier(SymbolID))
    return TokError("expected identifier in directive");

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  MCSymbol *Symbol = getContext().getOrCreateSymbol(SymbolID);

  Lex();
  getStreamer().emitCOFFSectionIndex(Symbol);
  return false;
}

bool VPEAsmParser::ParseDirectiveSymIdx(StringRef, SMLoc) {
  StringRef SymbolID;
  if (getParser().parseIdentifier(SymbolID))
    return TokError("expected identifier in directive");

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  MCSymbol *Symbol = getContext().getOrCreateSymbol(SymbolID);

  Lex();
  getStreamer().emitCOFFSymbolIndex(Symbol);
  return false;
}

/// ::= [ identifier ]
bool VPEAsmParser::parseCOMDATType(VPE::COMDATType &Type) {
  StringRef TypeId = getTok().getIdentifier();

  Type = StringSwitch<VPE::COMDATType>(TypeId)
    .Case("one_only", VPE::IMAGE_COMDAT_SELECT_NODUPLICATES)
    .Case("discard", VPE::IMAGE_COMDAT_SELECT_ANY)
    .Case("same_size", VPE::IMAGE_COMDAT_SELECT_SAME_SIZE)
    .Case("same_contents", VPE::IMAGE_COMDAT_SELECT_EXACT_MATCH)
    .Case("associative", VPE::IMAGE_COMDAT_SELECT_ASSOCIATIVE)
    .Case("largest", VPE::IMAGE_COMDAT_SELECT_LARGEST)
    .Case("newest", VPE::IMAGE_COMDAT_SELECT_NEWEST)
    .Default((VPE::COMDATType)0);

  if (Type == 0)
    return TokError(Twine("unrecognized COMDAT type '" + TypeId + "'"));

  Lex();

  return false;
}

/// ParseDirectiveLinkOnce
///  ::= .linkonce [ identifier ]
bool VPEAsmParser::ParseDirectiveLinkOnce(StringRef, SMLoc Loc) {
  VPE::COMDATType Type = VPE::IMAGE_COMDAT_SELECT_ANY;
  if (getLexer().is(AsmToken::Identifier))
    if (parseCOMDATType(Type))
      return true;

  const MCSectionVPE *Current =
      static_cast<const MCSectionVPE *>(getStreamer().getCurrentSectionOnly());

  if (Type == VPE::IMAGE_COMDAT_SELECT_ASSOCIATIVE)
    return Error(Loc, "cannot make section associative with .linkonce");

  if (Current->getCharacteristics() & VPE::IMAGE_SCN_LNK_COMDAT)
    return Error(Loc, Twine("section '") + Current->getName() +
                                                       "' is already linkonce");

  Current->setSelection(Type);

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in directive");

  return false;
}

namespace llvm {

MCAsmParserExtension *createVPEAsmParser() {
  return new VPEAsmParser;
}

} // end namespace llvm
