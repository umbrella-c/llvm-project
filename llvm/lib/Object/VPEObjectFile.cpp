//===- VPEObjectFile.cpp - VPE object file implementation ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the VPEObjectFile class.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/BinaryFormat/COFF.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/VPE.h"
#include "llvm/Object/Error.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/BinaryStreamReader.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <system_error>

using namespace llvm;
using namespace object;

using support::ulittle16_t;
using support::ulittle32_t;
using support::ulittle64_t;
using support::little16_t;

// Returns false if size is greater than the buffer size. And sets ec.
static bool checkSize(MemoryBufferRef M, std::error_code &EC, uint64_t Size) {
  if (M.getBufferSize() < Size) {
    EC = object_error::unexpected_eof;
    return false;
  }
  return true;
}

// Sets Obj unless any bytes in [addr, addr + size) fall outsize of m.
// Returns unexpected_eof if error.
template <typename T>
static Error getObject(const T *&Obj, MemoryBufferRef M,
                                 const void *Ptr,
                                 const uint64_t Size = sizeof(T)) {
  uintptr_t Addr = reinterpret_cast<uintptr_t>(Ptr);
  if (Error E = Binary::checkOffset(M, Addr, Size))
    return E;
  Obj = reinterpret_cast<const T *>(Addr);
  return Error::success();
}

// Decode a string table entry in base 64 (//AAAAAA). Expects \arg Str without
// prefixed slashes.
static bool decodeBase64StringEntry(StringRef Str, uint32_t &Result) {
  assert(Str.size() <= 6 && "String too long, possible overflow.");
  if (Str.size() > 6)
    return true;

  uint64_t Value = 0;
  while (!Str.empty()) {
    unsigned CharVal;
    if (Str[0] >= 'A' && Str[0] <= 'Z') // 0..25
      CharVal = Str[0] - 'A';
    else if (Str[0] >= 'a' && Str[0] <= 'z') // 26..51
      CharVal = Str[0] - 'a' + 26;
    else if (Str[0] >= '0' && Str[0] <= '9') // 52..61
      CharVal = Str[0] - '0' + 52;
    else if (Str[0] == '+') // 62
      CharVal = 62;
    else if (Str[0] == '/') // 63
      CharVal = 63;
    else
      return true;

    Value = (Value * 64) + CharVal;
    Str = Str.substr(1);
  }

  if (Value > std::numeric_limits<uint32_t>::max())
    return true;

  Result = static_cast<uint32_t>(Value);
  return false;
}

template <typename vpe_symbol_type>
const vpe_symbol_type *VPEObjectFile::toSymb(DataRefImpl Ref) const {
  const vpe_symbol_type *Addr =
      reinterpret_cast<const vpe_symbol_type *>(Ref.p);

  assert(!checkOffset(Data, uintptr_t(Addr), sizeof(*Addr)));
#ifndef NDEBUG
  // Verify that the symbol points to a valid entry in the symbol table.
  uintptr_t Offset = uintptr_t(Addr) - uintptr_t(base());

  assert((Offset - getPointerToSymbolTable()) % sizeof(vpe_symbol_type) == 0 &&
         "Symbol did not point to the beginning of a symbol");
#endif

  return Addr;
}

const vpe_section *VPEObjectFile::toSec(DataRefImpl Ref) const {
  const vpe_section *Addr = reinterpret_cast<const vpe_section*>(Ref.p);

#ifndef NDEBUG
  // Verify that the section points to a valid entry in the section table.
  if (Addr < SectionTable || Addr >= (SectionTable + getNumberOfSections()))
    report_fatal_error("Section was outside of section table.");

  uintptr_t Offset = uintptr_t(Addr) - uintptr_t(SectionTable);
  assert(Offset % sizeof(vpe_section) == 0 &&
         "Section did not point to the beginning of a section");
#endif

  return Addr;
}

void VPEObjectFile::moveSymbolNext(DataRefImpl &Ref) const {
  auto End = reinterpret_cast<uintptr_t>(StringTable);
  if (SymbolTable16) {
    const vpe_symbol16 *Symb = toSymb<vpe_symbol16>(Ref);
    Symb += 1 + Symb->NumberOfAuxSymbols;
    Ref.p = std::min(reinterpret_cast<uintptr_t>(Symb), End);
  } else if (SymbolTable32) {
    const vpe_symbol32 *Symb = toSymb<vpe_symbol32>(Ref);
    Symb += 1 + Symb->NumberOfAuxSymbols;
    Ref.p = std::min(reinterpret_cast<uintptr_t>(Symb), End);
  } else {
    llvm_unreachable("no symbol table pointer!");
  }
}

Expected<StringRef> VPEObjectFile::getSymbolName(DataRefImpl Ref) const {
  return getSymbolName(getVPESymbol(Ref));
}

uint64_t VPEObjectFile::getSymbolValueImpl(DataRefImpl Ref) const {
  return getVPESymbol(Ref).getValue();
}

uint32_t VPEObjectFile::getSymbolAlignment(DataRefImpl Ref) const {
  // MSVC/link.exe seems to align symbols to the next-power-of-2
  // up to 32 bytes.
  VPESymbolRef Symb = getVPESymbol(Ref);
  return std::min(uint64_t(32), PowerOf2Ceil(Symb.getValue()));
}

Expected<uint64_t> VPEObjectFile::getSymbolAddress(DataRefImpl Ref) const {
  uint64_t Result = cantFail(getSymbolValue(Ref));
  VPESymbolRef Symb = getVPESymbol(Ref);
  int32_t SectionNumber = Symb.getSectionNumber();

  if (Symb.isAnyUndefined() || Symb.isCommon() ||
      COFF::isReservedSectionNumber(SectionNumber))
    return Result;

  Expected<const vpe_section *> Section = getSection(SectionNumber);
  if (!Section)
    return Section.takeError();
  Result += (*Section)->VirtualAddress;

  // The section VirtualAddress does not include ImageBase, and we want to
  // return virtual addresses.
  Result += getImageBase();

  return Result;
}

Expected<SymbolRef::Type> VPEObjectFile::getSymbolType(DataRefImpl Ref) const {
  VPESymbolRef Symb = getVPESymbol(Ref);
  int32_t SectionNumber = Symb.getSectionNumber();

  if (Symb.getComplexType() == COFF::IMAGE_SYM_DTYPE_FUNCTION)
    return SymbolRef::ST_Function;
  if (Symb.isAnyUndefined())
    return SymbolRef::ST_Unknown;
  if (Symb.isCommon())
    return SymbolRef::ST_Data;
  if (Symb.isFileRecord())
    return SymbolRef::ST_File;

  // TODO: perhaps we need a new symbol type ST_Section.
  if (SectionNumber == COFF::IMAGE_SYM_DEBUG || Symb.isSectionDefinition())
    return SymbolRef::ST_Debug;

  if (!COFF::isReservedSectionNumber(SectionNumber))
    return SymbolRef::ST_Data;

  return SymbolRef::ST_Other;
}

Expected<uint32_t> VPEObjectFile::getSymbolFlags(DataRefImpl Ref) const {
  VPESymbolRef Symb = getVPESymbol(Ref);
  uint32_t Result = SymbolRef::SF_None;

  if (Symb.isExternal() || Symb.isWeakExternal())
    Result |= SymbolRef::SF_Global;

  if (const vpe_aux_weak_external *AWE = Symb.getWeakExternal()) {
    Result |= SymbolRef::SF_Weak;
    if (AWE->Characteristics != COFF::IMAGE_WEAK_EXTERN_SEARCH_ALIAS)
      Result |= SymbolRef::SF_Undefined;
  }

  if (Symb.getSectionNumber() == COFF::IMAGE_SYM_ABSOLUTE)
    Result |= SymbolRef::SF_Absolute;

  if (Symb.isFileRecord())
    Result |= SymbolRef::SF_FormatSpecific;

  if (Symb.isSectionDefinition())
    Result |= SymbolRef::SF_FormatSpecific;

  if (Symb.isCommon())
    Result |= SymbolRef::SF_Common;

  if (Symb.isUndefined())
    Result |= SymbolRef::SF_Undefined;

  return Result;
}

uint64_t VPEObjectFile::getCommonSymbolSizeImpl(DataRefImpl Ref) const {
  VPESymbolRef Symb = getVPESymbol(Ref);
  return Symb.getValue();
}

Expected<section_iterator>
VPEObjectFile::getSymbolSection(DataRefImpl Ref) const {
  VPESymbolRef Symb = getVPESymbol(Ref);
  if (COFF::isReservedSectionNumber(Symb.getSectionNumber()))
    return section_end();
  Expected<const vpe_section *> Sec = getSection(Symb.getSectionNumber());
  if (!Sec)
    return Sec.takeError();
  DataRefImpl Ret;
  Ret.p = reinterpret_cast<uintptr_t>(*Sec);
  return section_iterator(SectionRef(Ret, this));
}

unsigned VPEObjectFile::getSymbolSectionID(SymbolRef Sym) const {
  VPESymbolRef Symb = getVPESymbol(Sym.getRawDataRefImpl());
  return Symb.getSectionNumber();
}

void VPEObjectFile::moveSectionNext(DataRefImpl &Ref) const {
  const vpe_section *Sec = toSec(Ref);
  Sec += 1;
  Ref.p = reinterpret_cast<uintptr_t>(Sec);
}

Expected<StringRef> VPEObjectFile::getSectionName(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  return getSectionName(Sec);
}

uint64_t VPEObjectFile::getSectionAddress(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  uint64_t Result = Sec->VirtualAddress;

  // The section VirtualAddress does not include ImageBase, and we want to
  // return virtual addresses.
  Result += getImageBase();
  return Result;
}

uint64_t VPEObjectFile::getSectionIndex(DataRefImpl Sec) const {
  return toSec(Sec) - SectionTable;
}

uint64_t VPEObjectFile::getSectionSize(DataRefImpl Ref) const {
  return getSectionSize(toSec(Ref));
}

Expected<ArrayRef<uint8_t>>
VPEObjectFile::getSectionContents(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  ArrayRef<uint8_t> Res;
  if (Error E = getSectionContents(Sec, Res))
    return std::move(E);
  return Res;
}

uint64_t VPEObjectFile::getSectionAlignment(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  return Sec->getAlignment();
}

bool VPEObjectFile::isSectionCompressed(DataRefImpl Sec) const {
  return false;
}

bool VPEObjectFile::isSectionText(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  return Sec->Characteristics & COFF::IMAGE_SCN_CNT_CODE;
}

bool VPEObjectFile::isSectionData(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  return Sec->Characteristics & COFF::IMAGE_SCN_CNT_INITIALIZED_DATA;
}

bool VPEObjectFile::isSectionBSS(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  const uint32_t BssFlags = COFF::IMAGE_SCN_CNT_UNINITIALIZED_DATA |
                            COFF::IMAGE_SCN_MEM_READ |
                            COFF::IMAGE_SCN_MEM_WRITE;
  return (Sec->Characteristics & BssFlags) == BssFlags;
}

// The .debug sections are the only debug sections for COFF
// (\see MCObjectFileInfo.cpp).
bool VPEObjectFile::isDebugSection(DataRefImpl Ref) const {
  Expected<StringRef> SectionNameOrErr = getSectionName(Ref);
  if (!SectionNameOrErr) {
    // TODO: Report the error message properly.
    consumeError(SectionNameOrErr.takeError());
    return false;
  }
  StringRef SectionName = SectionNameOrErr.get();
  return SectionName.startswith(".debug");
}

unsigned VPEObjectFile::getSectionID(SectionRef Sec) const {
  uintptr_t Offset =
      uintptr_t(Sec.getRawDataRefImpl().p) - uintptr_t(SectionTable);
  assert((Offset % sizeof(vpe_section)) == 0);
  return (Offset / sizeof(vpe_section)) + 1;
}

bool VPEObjectFile::isSectionVirtual(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  // In COFF, a virtual section won't have any in-file
  // content, so the file pointer to the content will be zero.
  return Sec->PointerToRawData == 0;
}

static uint32_t getNumberOfRelocations(const vpe_section *Sec,
                                       MemoryBufferRef M, const uint8_t *base) {
  // The field for the number of relocations in COFF section table is only
  // 16-bit wide. If a section has more than 65535 relocations, 0xFFFF is set to
  // NumberOfRelocations field, and the actual relocation count is stored in the
  // VirtualAddress field in the first relocation entry.
  if (Sec->hasExtendedRelocations()) {
    const vpe_relocation *FirstReloc;
    if (getObject(FirstReloc, M, reinterpret_cast<const vpe_relocation*>(
        base + Sec->PointerToRelocations)))
      return 0;
    // -1 to exclude this first relocation entry.
    return FirstReloc->VirtualAddress - 1;
  }
  return Sec->NumberOfRelocations;
}

static const vpe_relocation *
getFirstReloc(const vpe_section *Sec, MemoryBufferRef M, const uint8_t *Base) {
  uint64_t NumRelocs = getNumberOfRelocations(Sec, M, Base);
  if (!NumRelocs)
    return nullptr;
  auto begin = reinterpret_cast<const vpe_relocation *>(
      Base + Sec->PointerToRelocations);
  if (Sec->hasExtendedRelocations()) {
    // Skip the first relocation entry repurposed to store the number of
    // relocations.
    begin++;
  }
  if (Binary::checkOffset(M, uintptr_t(begin),
                          sizeof(vpe_relocation) * NumRelocs))
    return nullptr;
  return begin;
}

relocation_iterator VPEObjectFile::section_rel_begin(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  const vpe_relocation *begin = getFirstReloc(Sec, Data, base());
  if (begin && Sec->VirtualAddress != 0)
    report_fatal_error("Sections with relocations should have an address of 0");
  DataRefImpl Ret;
  Ret.p = reinterpret_cast<uintptr_t>(begin);
  return relocation_iterator(RelocationRef(Ret, this));
}

relocation_iterator VPEObjectFile::section_rel_end(DataRefImpl Ref) const {
  const vpe_section *Sec = toSec(Ref);
  const vpe_relocation *I = getFirstReloc(Sec, Data, base());
  if (I)
    I += getNumberOfRelocations(Sec, Data, base());
  DataRefImpl Ret;
  Ret.p = reinterpret_cast<uintptr_t>(I);
  return relocation_iterator(RelocationRef(Ret, this));
}

// Initialize the pointer to the symbol table.
Error VPEObjectFile::initSymbolTablePtr() {
  if (VPEHeader)
    if (Error E = getObject(
            SymbolTable16, Data, base() + getPointerToSymbolTable(),
            (uint64_t)getNumberOfSymbols() * getSymbolTableEntrySize()))
      return E;

  if (VPEBigObjHeader)
    if (Error E = getObject(
            SymbolTable32, Data, base() + getPointerToSymbolTable(),
            (uint64_t)getNumberOfSymbols() * getSymbolTableEntrySize()))
      return E;

  // Find string table. The first four byte of the string table contains the
  // total size of the string table, including the size field itself. If the
  // string table is empty, the value of the first four byte would be 4.
  uint32_t StringTableOffset = getPointerToSymbolTable() +
                               getNumberOfSymbols() * getSymbolTableEntrySize();
  const uint8_t *StringTableAddr = base() + StringTableOffset;
  const ulittle32_t *StringTableSizePtr;
  if (Error E = getObject(StringTableSizePtr, Data, StringTableAddr))
    return E;
  StringTableSize = *StringTableSizePtr;
  if (Error E = getObject(StringTable, Data, StringTableAddr, StringTableSize))
    return E;

  // Treat table sizes < 4 as empty because contrary to the PECOFF spec, some
  // tools like cvtres write a size of 0 for an empty table instead of 4.
  if (StringTableSize < 4)
      StringTableSize = 4;

  // Check that the string table is null terminated if has any in it.
  if (StringTableSize > 4 && StringTable[StringTableSize - 1] != 0)
    return errorCodeToError(object_error::parse_failed);
  return Error::success();
}

uint64_t VPEObjectFile::getImageBase() const {
  if (PE32Header)
    return PE32Header->ImageBase;
  else if (PE32PlusHeader)
    return PE32PlusHeader->ImageBase;
  // This actually comes up in practice.
  return 0;
}

// Returns the file offset for the given VA.
Error VPEObjectFile::getVaPtr(uint64_t Addr, uintptr_t &Res) const {
  uint64_t ImageBase = getImageBase();
  uint64_t Rva = Addr - ImageBase;
  assert(Rva <= UINT32_MAX);
  return getRvaPtr((uint32_t)Rva, Res);
}

// Returns the file offset for the given RVA.
Error VPEObjectFile::getRvaPtr(uint32_t Addr, uintptr_t &Res) const {
  for (const SectionRef &S : sections()) {
    const vpe_section *Section = getVPESection(S);
    uint32_t SectionStart = Section->VirtualAddress;
    uint32_t SectionEnd = Section->VirtualAddress + Section->VirtualSize;
    if (SectionStart <= Addr && Addr < SectionEnd) {
      uint32_t Offset = Addr - SectionStart;
      Res = reinterpret_cast<uintptr_t>(base()) + Section->PointerToRawData +
            Offset;
      return Error::success();
    }
  }
  return errorCodeToError(object_error::parse_failed);
}

Error
VPEObjectFile::getRvaAndSizeAsBytes(uint32_t RVA, uint32_t Size,
                                     ArrayRef<uint8_t> &Contents) const {
  for (const SectionRef &S : sections()) {
    const vpe_section *Section = getVPESection(S);
    uint32_t SectionStart = Section->VirtualAddress;
    // Check if this RVA is within the section bounds. Be careful about integer
    // overflow.
    uint32_t OffsetIntoSection = RVA - SectionStart;
    if (SectionStart <= RVA && OffsetIntoSection < Section->VirtualSize &&
        Size <= Section->VirtualSize - OffsetIntoSection) {
      uintptr_t Begin =
          uintptr_t(base()) + Section->PointerToRawData + OffsetIntoSection;
      Contents =
          ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(Begin), Size);
      return Error::success();
    }
  }
  return errorCodeToError(object_error::parse_failed);
}

// Returns hint and name fields, assuming \p Rva is pointing to a Hint/Name
// table entry.
Error VPEObjectFile::getHintName(uint32_t Rva, uint16_t &Hint,
                                 StringRef &Name) const {
  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(Rva, IntPtr))
    return E;
  const uint8_t *Ptr = reinterpret_cast<const uint8_t *>(IntPtr);
  Hint = *reinterpret_cast<const ulittle16_t *>(Ptr);
  Name = StringRef(reinterpret_cast<const char *>(Ptr + 2));
  return Error::success();
}

Error
VPEObjectFile::getDebugPDBInfo(const vpe_debug_directory *DebugDir,
                               const codeview::DebugInfo *&PDBInfo,
                               StringRef &PDBFileName) const {
  ArrayRef<uint8_t> InfoBytes;
  if (Error E = getRvaAndSizeAsBytes(
          DebugDir->AddressOfRawData, DebugDir->SizeOfData, InfoBytes))
    return E;
  if (InfoBytes.size() < sizeof(*PDBInfo) + 1)
    return errorCodeToError(object_error::parse_failed);
  PDBInfo = reinterpret_cast<const codeview::DebugInfo *>(InfoBytes.data());
  InfoBytes = InfoBytes.drop_front(sizeof(*PDBInfo));
  PDBFileName = StringRef(reinterpret_cast<const char *>(InfoBytes.data()),
                          InfoBytes.size());
  // Truncate the name at the first null byte. Ignore any padding.
  PDBFileName = PDBFileName.split('\0').first;
  return Error::success();
}

Error
VPEObjectFile::getDebugPDBInfo(const codeview::DebugInfo *&PDBInfo,
                                StringRef &PDBFileName) const {
  for (const vpe_debug_directory &D : debug_directories())
    if (D.Type == COFF::IMAGE_DEBUG_TYPE_CODEVIEW)
      return getDebugPDBInfo(&D, PDBInfo, PDBFileName);
  // If we get here, there is no PDB info to return.
  PDBInfo = nullptr;
  PDBFileName = StringRef();
  return Error::success();
}

// Find the import table.
Error VPEObjectFile::initImportTablePtr() {
  // First, we get the RVA of the import table. If the file lacks a pointer to
  // the import table, do nothing.
  const vpe_data_directory *DataEntry = getDataDirectory(COFF::IMPORT_TABLE);
  if (!DataEntry)
    return Error::success();

  // Do nothing if the pointer to import table is NULL.
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();

  uint32_t ImportTableRva = DataEntry->RelativeVirtualAddress;

  // Find the section that contains the RVA. This is needed because the RVA is
  // the import table's memory address which is different from its file offset.
  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(ImportTableRva, IntPtr))
    return E;
  if (Error E = checkOffset(Data, IntPtr, DataEntry->Size))
    return E;
  ImportDirectory = reinterpret_cast<
      const vpe_import_directory_table_entry *>(IntPtr);
    return Error::success();
}

// Initializes DelayImportDirectory and NumberOfDelayImportDirectory.
Error VPEObjectFile::initDelayImportTablePtr() {
  const vpe_data_directory *DataEntry =
      getDataDirectory(COFF::DELAY_IMPORT_DESCRIPTOR);
  if (!DataEntry)
    return Error::success();
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();

  uint32_t RVA = DataEntry->RelativeVirtualAddress;
  NumberOfDelayImportDirectory = DataEntry->Size /
      sizeof(vpe_delay_import_directory_table_entry) - 1;

  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(RVA, IntPtr))
    return E;
  DelayImportDirectory = reinterpret_cast<
      const vpe_delay_import_directory_table_entry *>(IntPtr);
  return Error::success();
}

// Find the export table.
Error VPEObjectFile::initExportTablePtr() {
  // First, we get the RVA of the export table. If the file lacks a pointer to
  // the export table, do nothing.
  const vpe_data_directory *DataEntry = getDataDirectory(COFF::EXPORT_TABLE);
  if (!DataEntry)
    return Error::success();

  // Do nothing if the pointer to export table is NULL.
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();

  uint32_t ExportTableRva = DataEntry->RelativeVirtualAddress;
  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(ExportTableRva, IntPtr))
    return E;
  ExportDirectory =
      reinterpret_cast<const vpe_export_directory_table_entry *>(IntPtr);
  return Error::success();
}

Error VPEObjectFile::initBaseRelocPtr() {
  const vpe_data_directory *DataEntry =
      getDataDirectory(COFF::BASE_RELOCATION_TABLE);
  if (!DataEntry)
    return Error::success();
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();

  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(DataEntry->RelativeVirtualAddress, IntPtr))
    return E;
  BaseRelocHeader = reinterpret_cast<const vpe_base_reloc_block_header *>(
      IntPtr);
  BaseRelocEnd = reinterpret_cast<vpe_base_reloc_block_header *>(
      IntPtr + DataEntry->Size);
  // FIXME: Verify the section containing BaseRelocHeader has at least
  // DataEntry->Size bytes after DataEntry->RelativeVirtualAddress.
  return Error::success();
}

Error VPEObjectFile::initDebugDirectoryPtr() {
  // Get the RVA of the debug directory. Do nothing if it does not exist.
  const vpe_data_directory *DataEntry = getDataDirectory(COFF::DEBUG_DIRECTORY);
  if (!DataEntry)
    return Error::success();

  // Do nothing if the RVA is NULL.
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();

  // Check that the size is a multiple of the entry size.
  if (DataEntry->Size % sizeof(vpe_debug_directory) != 0)
    return errorCodeToError(object_error::parse_failed);

  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(DataEntry->RelativeVirtualAddress, IntPtr))
    return E;
  DebugDirectoryBegin = reinterpret_cast<const vpe_debug_directory *>(IntPtr);
  DebugDirectoryEnd = reinterpret_cast<const vpe_debug_directory *>(
      IntPtr + DataEntry->Size);
  // FIXME: Verify the section containing DebugDirectoryBegin has at least
  // DataEntry->Size bytes after DataEntry->RelativeVirtualAddress.
  return Error::success();
}

Error VPEObjectFile::initTLSDirectoryPtr() {
  // Get the RVA of the TLS directory. Do nothing if it does not exist.
  const vpe_data_directory *DataEntry = getDataDirectory(COFF::TLS_TABLE);
  if (!DataEntry)
    return Error::success();

  // Do nothing if the RVA is NULL.
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();

  uint64_t DirSize =
      is64() ? sizeof(vpe_tls_directory64) : sizeof(vpe_tls_directory32);

  // Check that the size is correct.
  if (DataEntry->Size != DirSize)
    return createStringError(
        object_error::parse_failed,
        "TLS Directory size (%u) is not the expected size (%" PRIu64 ").",
        static_cast<uint32_t>(DataEntry->Size), DirSize);

  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(DataEntry->RelativeVirtualAddress, IntPtr))
    return E;

  if (is64())
    TLSDirectory64 = reinterpret_cast<const vpe_tls_directory64 *>(IntPtr);
  else
    TLSDirectory32 = reinterpret_cast<const vpe_tls_directory32 *>(IntPtr);

  return Error::success();
}

Error VPEObjectFile::initLoadConfigPtr() {
  // Get the RVA of the debug directory. Do nothing if it does not exist.
  const vpe_data_directory *DataEntry = getDataDirectory(COFF::LOAD_CONFIG_TABLE);
  if (!DataEntry)
    return Error::success();

  // Do nothing if the RVA is NULL.
  if (DataEntry->RelativeVirtualAddress == 0)
    return Error::success();
  uintptr_t IntPtr = 0;
  if (Error E = getRvaPtr(DataEntry->RelativeVirtualAddress, IntPtr))
    return E;

  LoadConfig = (const void *)IntPtr;
  return Error::success();
}

Expected<std::unique_ptr<VPEObjectFile>>
VPEObjectFile::create(MemoryBufferRef Object) {
  std::unique_ptr<VPEObjectFile> Obj(new VPEObjectFile(std::move(Object)));
  if (Error E = Obj->initialize())
    return std::move(E);
  return std::move(Obj);
}

VPEObjectFile::VPEObjectFile(MemoryBufferRef Object)
    : ObjectFile(Binary::ID_VPE, Object), VPEHeader(nullptr),
      VPEBigObjHeader(nullptr), PE32Header(nullptr), PE32PlusHeader(nullptr),
      DataDirectory(nullptr), SectionTable(nullptr), SymbolTable16(nullptr),
      SymbolTable32(nullptr), StringTable(nullptr), StringTableSize(0),
      ImportDirectory(nullptr), DelayImportDirectory(nullptr), 
      NumberOfDelayImportDirectory(0), ExportDirectory(nullptr), 
      BaseRelocHeader(nullptr), BaseRelocEnd(nullptr),
      DebugDirectoryBegin(nullptr), DebugDirectoryEnd(nullptr),
      TLSDirectory32(nullptr), TLSDirectory64(nullptr) { }


Error VPEObjectFile::initialize() {
  // Check that we at least have enough room for a header.
  std::error_code EC;
  if (!checkSize(Data, EC, sizeof(vpe_file_header)))
    return errorCodeToError(EC);

  // The current location in the file where we are looking at.
  uint64_t CurPtr = 0;

  // PE header is optional and is present only in executables. If it exists,
  // it is placed right after COFF header.
  bool HasPEHeader = false;

  // Check if this is a PE/COFF file.
  if (checkSize(Data, EC, sizeof(vpe_dos_header) + sizeof(COFF::PEMagic))) {
    // PE/COFF, seek through MS-DOS compatibility stub and 4-byte
    // PE signature to find 'normal' COFF header.
    const auto *DH = reinterpret_cast<const vpe_dos_header *>(base());
    if (DH->Magic[0] == 'M' && DH->Magic[1] == 'Z') {
      CurPtr = DH->AddressOfNewExeHeader;
      // Check the PE magic bytes. ("PE\0\0")
      if (memcmp(base() + CurPtr, COFF::PEMagic, sizeof(COFF::PEMagic)) != 0) {
        return errorCodeToError(object_error::parse_failed);
      }
      CurPtr += sizeof(COFF::PEMagic); // Skip the PE magic bytes.
      HasPEHeader = true;
    }
  }

  if (Error E = getObject(VPEHeader, Data, base() + CurPtr))
    return E;

  // It might be a bigobj file, let's check.  Note that COFF bigobj and COFF
  // import libraries share a common prefix but bigobj is more restrictive.
  if (!HasPEHeader && VPEHeader->Machine == COFF::IMAGE_FILE_MACHINE_UNKNOWN &&
      VPEHeader->NumberOfSections == uint16_t(0xffff) &&
      checkSize(Data, EC, sizeof(vpe_bigobj_file_header))) {
    if (Error E = getObject(VPEBigObjHeader, Data, base() + CurPtr))
      return E;

    // Verify that we are dealing with bigobj.
    if (VPEBigObjHeader->Version >= COFF::BigObjHeader::MinBigObjectVersion &&
        std::memcmp(VPEBigObjHeader->UUID, COFF::BigObjMagic,
                    sizeof(COFF::BigObjMagic)) == 0) {
      VPEHeader = nullptr;
      CurPtr += sizeof(vpe_bigobj_file_header);
    } else {
      // It's not a bigobj.
      VPEBigObjHeader = nullptr;
    }
  }
  if (VPEHeader) {
    // The prior checkSize call may have failed.  This isn't a hard error
    // because we were just trying to sniff out bigobj.
    EC = std::error_code();
    CurPtr += sizeof(vpe_file_header);

    if (VPEHeader->isImportLibrary())
      return errorCodeToError(EC);
  }

  if (HasPEHeader) {
    const vpe_pe32_header *Header;
    if (Error E = getObject(Header, Data, base() + CurPtr))
      return E;

    const uint8_t *DataDirAddr;
    uint64_t DataDirSize;
    if (Header->Magic == COFF::PE32Header::PE32) {
      PE32Header = Header;
      DataDirAddr = base() + CurPtr + sizeof(vpe_pe32_header);
      DataDirSize = sizeof(vpe_data_directory) * PE32Header->NumberOfRvaAndSize;
    } else if (Header->Magic == COFF::PE32Header::PE32_PLUS) {
      PE32PlusHeader = reinterpret_cast<const vpe_pe32plus_header *>(Header);
      DataDirAddr = base() + CurPtr + sizeof(vpe_pe32plus_header);
      DataDirSize = sizeof(vpe_data_directory) * PE32PlusHeader->NumberOfRvaAndSize;
    } else {
      // It's neither PE32 nor PE32+.
      return errorCodeToError(object_error::parse_failed);
    }
    if (Error E = getObject(DataDirectory, Data, DataDirAddr, DataDirSize))
      return E;
  }

  if (VPEHeader)
    CurPtr += VPEHeader->SizeOfOptionalHeader;

  assert(VPEHeader || VPEBigObjHeader);

  if (Error E =
          getObject(SectionTable, Data, base() + CurPtr,
                    (uint64_t)getNumberOfSections() * sizeof(vpe_section)))
    return E;

  // Initialize the pointer to the symbol table.
  if (getPointerToSymbolTable() != 0) {
    if (Error E = initSymbolTablePtr()) {
      // Recover from errors reading the symbol table.
      consumeError(std::move(E));
      SymbolTable16 = nullptr;
      SymbolTable32 = nullptr;
      StringTable = nullptr;
      StringTableSize = 0;
    }
  } else {
    // We had better not have any symbols if we don't have a symbol table.
    if (getNumberOfSymbols() != 0) {
      return errorCodeToError(object_error::parse_failed);
    }
  }

  // Initialize the pointer to the beginning of the import table.
  if (Error E = initImportTablePtr())
    return E;
  if (Error E = initDelayImportTablePtr())
    return E;

  // Initialize the pointer to the export table.
  if (Error E = initExportTablePtr())
    return E;

  // Initialize the pointer to the base relocation table.
  if (Error E = initBaseRelocPtr())
    return E;

  // Initialize the pointer to the debug directory.
  if (Error E = initDebugDirectoryPtr())
    return E;

  // Initialize the pointer to the TLS directory.
  if (Error E = initTLSDirectoryPtr())
    return E;

  if (Error E = initLoadConfigPtr())
    return E;

  return Error::success();
}

basic_symbol_iterator VPEObjectFile::symbol_begin() const {
  DataRefImpl Ret;
  Ret.p = getSymbolTable();
  return basic_symbol_iterator(SymbolRef(Ret, this));
}

basic_symbol_iterator VPEObjectFile::symbol_end() const {
  // The symbol table ends where the string table begins.
  DataRefImpl Ret;
  Ret.p = reinterpret_cast<uintptr_t>(StringTable);
  return basic_symbol_iterator(SymbolRef(Ret, this));
}

vpe_import_directory_iterator VPEObjectFile::import_directory_begin() const {
  if (!ImportDirectory)
    return import_directory_end();
  if (ImportDirectory->isNull())
    return import_directory_end();
  return vpe_import_directory_iterator(
      VPEImportDirectoryEntryRef(ImportDirectory, 0, this));
}

vpe_import_directory_iterator VPEObjectFile::import_directory_end() const {
  return vpe_import_directory_iterator(
      VPEImportDirectoryEntryRef(nullptr, -1, this));
}

vpe_delay_import_directory_iterator
VPEObjectFile::delay_import_directory_begin() const {
  return vpe_delay_import_directory_iterator(
      VPEDelayImportDirectoryEntryRef(DelayImportDirectory, 0, this));
}

vpe_delay_import_directory_iterator
VPEObjectFile::delay_import_directory_end() const {
  return vpe_delay_import_directory_iterator(
      VPEDelayImportDirectoryEntryRef(
          DelayImportDirectory, NumberOfDelayImportDirectory, this));
}

vpe_export_directory_iterator VPEObjectFile::export_directory_begin() const {
  return vpe_export_directory_iterator(
      VPEExportDirectoryEntryRef(ExportDirectory, 0, this));
}

vpe_export_directory_iterator VPEObjectFile::export_directory_end() const {
  if (!ExportDirectory)
    return vpe_export_directory_iterator(
        VPEExportDirectoryEntryRef(nullptr, 0, this));
  VPEExportDirectoryEntryRef Ref(ExportDirectory,
                              ExportDirectory->AddressTableEntries, this);
  return vpe_export_directory_iterator(Ref);
}

section_iterator VPEObjectFile::section_begin() const {
  DataRefImpl Ret;
  Ret.p = reinterpret_cast<uintptr_t>(SectionTable);
  return section_iterator(SectionRef(Ret, this));
}

section_iterator VPEObjectFile::section_end() const {
  DataRefImpl Ret;
  int NumSections =
      VPEHeader && VPEHeader->isImportLibrary() ? 0 : getNumberOfSections();
  Ret.p = reinterpret_cast<uintptr_t>(SectionTable + NumSections);
  return section_iterator(SectionRef(Ret, this));
}

vpe_base_reloc_iterator VPEObjectFile::base_reloc_begin() const {
  return vpe_base_reloc_iterator(VPEBaseRelocRef(BaseRelocHeader, this));
}

vpe_base_reloc_iterator VPEObjectFile::base_reloc_end() const {
  return vpe_base_reloc_iterator(VPEBaseRelocRef(BaseRelocEnd, this));
}

uint8_t VPEObjectFile::getBytesInAddress() const {
  return getArch() == Triple::x86_64 || getArch() == Triple::aarch64 ? 8 : 4;
}

StringRef VPEObjectFile::getFileFormatName() const {
  switch(getMachine()) {
  case COFF::IMAGE_FILE_MACHINE_I386:
    return "VPE-i386";
  case COFF::IMAGE_FILE_MACHINE_AMD64:
    return "VPE-x86-64";
  case COFF::IMAGE_FILE_MACHINE_ARMNT:
    return "VPE-ARM";
  case COFF::IMAGE_FILE_MACHINE_ARM64:
    return "VPE-ARM64";
  default:
    return "VPE-<unknown arch>";
  }
}

Triple::ArchType VPEObjectFile::getArch() const {
  switch (getMachine()) {
  case COFF::IMAGE_FILE_MACHINE_I386:
    return Triple::x86;
  case COFF::IMAGE_FILE_MACHINE_AMD64:
    return Triple::x86_64;
  case COFF::IMAGE_FILE_MACHINE_ARMNT:
    return Triple::thumb;
  case COFF::IMAGE_FILE_MACHINE_ARM64:
    return Triple::aarch64;
  default:
    return Triple::UnknownArch;
  }
}

Expected<uint64_t> VPEObjectFile::getStartAddress() const {
  if (PE32Header)
    return PE32Header->AddressOfEntryPoint;
  return 0;
}

iterator_range<vpe_import_directory_iterator>
VPEObjectFile::import_directories() const {
  return make_range(import_directory_begin(), import_directory_end());
}

iterator_range<vpe_delay_import_directory_iterator>
VPEObjectFile::delay_import_directories() const {
  return make_range(delay_import_directory_begin(),
                    delay_import_directory_end());
}

iterator_range<vpe_export_directory_iterator>
VPEObjectFile::export_directories() const {
  return make_range(export_directory_begin(), export_directory_end());
}

iterator_range<vpe_base_reloc_iterator> VPEObjectFile::base_relocs() const {
  return make_range(base_reloc_begin(), base_reloc_end());
}

const vpe_data_directory *VPEObjectFile::getDataDirectory(uint32_t Index) const {
  if (!DataDirectory)
    return nullptr;
  assert(PE32Header || PE32PlusHeader);
  uint32_t NumEnt = PE32Header ? PE32Header->NumberOfRvaAndSize
                               : PE32PlusHeader->NumberOfRvaAndSize;
  if (Index >= NumEnt)
    return nullptr;
  return &DataDirectory[Index];
}

Expected<const vpe_section *> VPEObjectFile::getSection(int32_t Index) const {
  // Perhaps getting the section of a reserved section index should be an error,
  // but callers rely on this to return null.
  if (COFF::isReservedSectionNumber(Index))
    return (const vpe_section *)nullptr;
  if (static_cast<uint32_t>(Index) <= getNumberOfSections()) {
    // We already verified the section table data, so no need to check again.
    return SectionTable + (Index - 1);
  }
  return errorCodeToError(object_error::parse_failed);
}

Expected<StringRef> VPEObjectFile::getString(uint32_t Offset) const {
  if (StringTableSize <= 4)
    // Tried to get a string from an empty string table.
    return errorCodeToError(object_error::parse_failed);
  if (Offset >= StringTableSize)
    return errorCodeToError(object_error::unexpected_eof);
  return StringRef(StringTable + Offset);
}

Expected<StringRef> VPEObjectFile::getSymbolName(VPESymbolRef Symbol) const {
  return getSymbolName(Symbol.getGeneric());
}

Expected<StringRef>
VPEObjectFile::getSymbolName(const vpe_symbol_generic *Symbol) const {
  // Check for string table entry. First 4 bytes are 0.
  if (Symbol->Name.Offset.Zeroes == 0)
    return getString(Symbol->Name.Offset.Offset);

  // Null terminated, let ::strlen figure out the length.
  if (Symbol->Name.ShortName[COFF::NameSize - 1] == 0)
    return StringRef(Symbol->Name.ShortName);

  // Not null terminated, use all 8 bytes.
  return StringRef(Symbol->Name.ShortName, COFF::NameSize);
}

ArrayRef<uint8_t>
VPEObjectFile::getSymbolAuxData(VPESymbolRef Symbol) const {
  const uint8_t *Aux = nullptr;

  size_t SymbolSize = getSymbolTableEntrySize();
  if (Symbol.getNumberOfAuxSymbols() > 0) {
    // AUX data comes immediately after the symbol in COFF
    Aux = reinterpret_cast<const uint8_t *>(Symbol.getRawPtr()) + SymbolSize;
#ifndef NDEBUG
    // Verify that the Aux symbol points to a valid entry in the symbol table.
    uintptr_t Offset = uintptr_t(Aux) - uintptr_t(base());
    if (Offset < getPointerToSymbolTable() ||
        Offset >=
            getPointerToSymbolTable() + (getNumberOfSymbols() * SymbolSize))
      report_fatal_error("Aux Symbol data was outside of symbol table.");

    assert((Offset - getPointerToSymbolTable()) % SymbolSize == 0 &&
           "Aux Symbol data did not point to the beginning of a symbol");
#endif
  }
  return makeArrayRef(Aux, Symbol.getNumberOfAuxSymbols() * SymbolSize);
}

uint32_t VPEObjectFile::getSymbolIndex(VPESymbolRef Symbol) const {
  uintptr_t Offset =
      reinterpret_cast<uintptr_t>(Symbol.getRawPtr()) - getSymbolTable();
  assert(Offset % getSymbolTableEntrySize() == 0 &&
         "Symbol did not point to the beginning of a symbol");
  size_t Index = Offset / getSymbolTableEntrySize();
  assert(Index < getNumberOfSymbols());
  return Index;
}

Expected<StringRef> VPEObjectFile::getSectionName(const vpe_section *Sec) const {
  StringRef Name;
  if (Sec->Name[COFF::NameSize - 1] == 0)
    // Null terminated, let ::strlen figure out the length.
    Name = Sec->Name;
  else
    // Not null terminated, use all 8 bytes.
    Name = StringRef(Sec->Name, COFF::NameSize);

  // Check for string table entry. First byte is '/'.
  if (Name.startswith("/")) {
    uint32_t Offset;
    if (Name.startswith("//")) {
      if (decodeBase64StringEntry(Name.substr(2), Offset))
        return createStringError(object_error::parse_failed,
                                 "invalid section name");
    } else {
      if (Name.substr(1).getAsInteger(10, Offset))
        return createStringError(object_error::parse_failed,
                                 "invalid section name");
    }
    return getString(Offset);
  }

  return Name;
}

uint64_t VPEObjectFile::getSectionSize(const vpe_section *Sec) const {
  // SizeOfRawData and VirtualSize change what they represent depending on
  // whether or not we have an executable image.
  //
  // For object files, SizeOfRawData contains the size of section's data;
  // VirtualSize should be zero but isn't due to buggy COFF writers.
  //
  // For executables, SizeOfRawData *must* be a multiple of FileAlignment; the
  // actual section size is in VirtualSize.  It is possible for VirtualSize to
  // be greater than SizeOfRawData; the contents past that point should be
  // considered to be zero.
  if (getDOSHeader())
    return std::min(Sec->VirtualSize, Sec->SizeOfRawData);
  return Sec->SizeOfRawData;
}

Error
VPEObjectFile::getSectionContents(const vpe_section *Sec,
                                  ArrayRef<uint8_t> &Res) const {
  // In COFF, a virtual section won't have any in-file
  // content, so the file pointer to the content will be zero.
  if (Sec->PointerToRawData == 0)
    return Error::success();
  // The only thing that we need to verify is that the contents is contained
  // within the file bounds. We don't need to make sure it doesn't cover other
  // data, as there's nothing that says that is not allowed.
  uintptr_t ConStart =
      reinterpret_cast<uintptr_t>(base()) + Sec->PointerToRawData;
  uint32_t SectionSize = getSectionSize(Sec);
  if (Error E = checkOffset(Data, ConStart, SectionSize))
    return E;
  Res = makeArrayRef(reinterpret_cast<const uint8_t *>(ConStart), SectionSize);
  return Error::success();
}

const vpe_relocation *VPEObjectFile::toRel(DataRefImpl Rel) const {
  return reinterpret_cast<const vpe_relocation*>(Rel.p);
}

void VPEObjectFile::moveRelocationNext(DataRefImpl &Rel) const {
  Rel.p = reinterpret_cast<uintptr_t>(
            reinterpret_cast<const vpe_relocation*>(Rel.p) + 1);
}

uint64_t VPEObjectFile::getRelocationOffset(DataRefImpl Rel) const {
  const vpe_relocation *R = toRel(Rel);
  return R->VirtualAddress;
}

symbol_iterator VPEObjectFile::getRelocationSymbol(DataRefImpl Rel) const {
  const vpe_relocation *R = toRel(Rel);
  DataRefImpl Ref;
  if (R->SymbolTableIndex >= getNumberOfSymbols())
    return symbol_end();
  if (SymbolTable16)
    Ref.p = reinterpret_cast<uintptr_t>(SymbolTable16 + R->SymbolTableIndex);
  else if (SymbolTable32)
    Ref.p = reinterpret_cast<uintptr_t>(SymbolTable32 + R->SymbolTableIndex);
  else
    llvm_unreachable("no symbol table pointer!");
  return symbol_iterator(SymbolRef(Ref, this));
}

uint64_t VPEObjectFile::getRelocationType(DataRefImpl Rel) const {
  const vpe_relocation* R = toRel(Rel);
  return R->Type;
}

const vpe_section *
VPEObjectFile::getVPESection(const SectionRef &Section) const {
  return toSec(Section.getRawDataRefImpl());
}

VPESymbolRef VPEObjectFile::getVPESymbol(const DataRefImpl &Ref) const {
  if (SymbolTable16)
    return toSymb<vpe_symbol16>(Ref);
  if (SymbolTable32)
    return toSymb<vpe_symbol32>(Ref);
  llvm_unreachable("no symbol table pointer!");
}

VPESymbolRef VPEObjectFile::getVPESymbol(const SymbolRef &Symbol) const {
  return getVPESymbol(Symbol.getRawDataRefImpl());
}

const vpe_relocation *
VPEObjectFile::getVPERelocation(const RelocationRef &Reloc) const {
  return toRel(Reloc.getRawDataRefImpl());
}

ArrayRef<vpe_relocation>
VPEObjectFile::getRelocations(const vpe_section *Sec) const {
  return {getFirstReloc(Sec, Data, base()),
          getNumberOfRelocations(Sec, Data, base())};
}

#define LLVM_vpe_SWITCH_RELOC_TYPE_NAME(reloc_type)                           \
  case COFF::reloc_type:                                                       \
    return #reloc_type;

StringRef VPEObjectFile::getRelocationTypeName(uint16_t Type) const {
  switch (getMachine()) {
  case COFF::IMAGE_FILE_MACHINE_AMD64:
    switch (Type) {
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_ABSOLUTE);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_ADDR64);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_ADDR32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_ADDR32NB);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_REL32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_REL32_1);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_REL32_2);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_REL32_3);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_REL32_4);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_REL32_5);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_SECTION);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_SECREL);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_SECREL7);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_TOKEN);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_SREL32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_PAIR);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_AMD64_SSPAN32);
    default:
      return "Unknown";
    }
    break;
  case COFF::IMAGE_FILE_MACHINE_ARMNT:
    switch (Type) {
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_ABSOLUTE);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_ADDR32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_ADDR32NB);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BRANCH24);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BRANCH11);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_TOKEN);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BLX24);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BLX11);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_REL32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_SECTION);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_SECREL);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_MOV32A);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_MOV32T);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BRANCH20T);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BRANCH24T);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_BLX23T);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM_PAIR);
    default:
      return "Unknown";
    }
    break;
  case COFF::IMAGE_FILE_MACHINE_ARM64:
    switch (Type) {
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_ABSOLUTE);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_ADDR32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_ADDR32NB);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_BRANCH26);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_PAGEBASE_REL21);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_REL21);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_PAGEOFFSET_12A);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_PAGEOFFSET_12L);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_SECREL);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_SECREL_LOW12A);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_SECREL_HIGH12A);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_SECREL_LOW12L);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_TOKEN);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_SECTION);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_ADDR64);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_BRANCH19);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_BRANCH14);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_ARM64_REL32);
    default:
      return "Unknown";
    }
    break;
  case COFF::IMAGE_FILE_MACHINE_I386:
    switch (Type) {
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_ABSOLUTE);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_DIR16);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_REL16);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_DIR32);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_DIR32NB);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_SEG12);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_SECTION);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_SECREL);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_TOKEN);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_SECREL7);
    LLVM_vpe_SWITCH_RELOC_TYPE_NAME(IMAGE_REL_I386_REL32);
    default:
      return "Unknown";
    }
    break;
  default:
    return "Unknown";
  }
}

#undef LLVM_vpe_SWITCH_RELOC_TYPE_NAME

void VPEObjectFile::getRelocationTypeName(
    DataRefImpl Rel, SmallVectorImpl<char> &Result) const {
  const vpe_relocation *Reloc = toRel(Rel);
  StringRef Res = getRelocationTypeName(Reloc->Type);
  Result.append(Res.begin(), Res.end());
}

bool VPEObjectFile::isRelocatableObject() const {
  return !DataDirectory;
}

StringRef VPEObjectFile::mapDebugSectionName(StringRef Name) const {
  return StringSwitch<StringRef>(Name)
      .Case("eh_fram", "eh_frame")
      .Default(Name);
}

bool VPEImportDirectoryEntryRef::
operator==(const VPEImportDirectoryEntryRef &Other) const {
  return ImportTable == Other.ImportTable && Index == Other.Index;
}

void VPEImportDirectoryEntryRef::moveNext() {
  ++Index;
  if (ImportTable[Index].isNull()) {
    Index = -1;
    ImportTable = nullptr;
  }
}

Error VPEImportDirectoryEntryRef::getImportTableEntry(
    const vpe_import_directory_table_entry *&Result) const {
  return getObject(Result, OwningObject->Data, ImportTable + Index);
}

static vpe_imported_symbol_iterator
makeImportedSymbolIterator(const VPEObjectFile *Object,
                           uintptr_t Ptr, int Index) {
  if (Object->getBytesInAddress() == 4) {
    auto *P = reinterpret_cast<const vpe_import_lookup_table_entry32 *>(Ptr);
    return vpe_imported_symbol_iterator(VPEImportedSymbolRef(P, Index, Object));
  }
  auto *P = reinterpret_cast<const vpe_import_lookup_table_entry64 *>(Ptr);
  return vpe_imported_symbol_iterator(VPEImportedSymbolRef(P, Index, Object));
}

static vpe_imported_symbol_iterator
importedSymbolBegin(uint32_t RVA, const VPEObjectFile *Object) {
  uintptr_t IntPtr = 0;
  // FIXME: Handle errors.
  cantFail(Object->getRvaPtr(RVA, IntPtr));
  return makeImportedSymbolIterator(Object, IntPtr, 0);
}

static vpe_imported_symbol_iterator
importedSymbolEnd(uint32_t RVA, const VPEObjectFile *Object) {
  uintptr_t IntPtr = 0;
  // FIXME: Handle errors.
  cantFail(Object->getRvaPtr(RVA, IntPtr));
  // Forward the pointer to the last entry which is null.
  int Index = 0;
  if (Object->getBytesInAddress() == 4) {
    auto *Entry = reinterpret_cast<ulittle32_t *>(IntPtr);
    while (*Entry++)
      ++Index;
  } else {
    auto *Entry = reinterpret_cast<ulittle64_t *>(IntPtr);
    while (*Entry++)
      ++Index;
  }
  return makeImportedSymbolIterator(Object, IntPtr, Index);
}

vpe_imported_symbol_iterator
VPEImportDirectoryEntryRef::imported_symbol_begin() const {
  return importedSymbolBegin(ImportTable[Index].ImportAddressTableRVA,
                             OwningObject);
}

vpe_imported_symbol_iterator
VPEImportDirectoryEntryRef::imported_symbol_end() const {
  return importedSymbolEnd(ImportTable[Index].ImportAddressTableRVA,
                           OwningObject);
}

iterator_range<vpe_imported_symbol_iterator>
VPEImportDirectoryEntryRef::imported_symbols() const {
  return make_range(imported_symbol_begin(), imported_symbol_end());
}

vpe_imported_symbol_iterator
VPEImportDirectoryEntryRef::lookup_table_begin() const {
  return importedSymbolBegin(ImportTable[Index].ImportLookupTableRVA,
                             OwningObject);
}

vpe_imported_symbol_iterator
VPEImportDirectoryEntryRef::lookup_table_end() const {
  return importedSymbolEnd(ImportTable[Index].ImportLookupTableRVA,
                           OwningObject);
}

iterator_range<vpe_imported_symbol_iterator>
VPEImportDirectoryEntryRef::lookup_table_symbols() const {
  return make_range(lookup_table_begin(), lookup_table_end());
}

Error VPEImportDirectoryEntryRef::getName(StringRef &Result) const {
  uintptr_t IntPtr = 0;
  if (Error E = OwningObject->getRvaPtr(ImportTable[Index].NameRVA, IntPtr))
    return E;
  Result = StringRef(reinterpret_cast<const char *>(IntPtr));
  return Error::success();
}

Error
VPEImportDirectoryEntryRef::getImportLookupTableRVA(uint32_t &Result) const {
  Result = ImportTable[Index].ImportLookupTableRVA;
  return Error::success();
}

Error
VPEImportDirectoryEntryRef::getImportAddressTableRVA(uint32_t &Result) const {
  Result = ImportTable[Index].ImportAddressTableRVA;
  return Error::success();
}

bool VPEDelayImportDirectoryEntryRef::
operator==(const VPEDelayImportDirectoryEntryRef &Other) const {
  return Table == Other.Table && Index == Other.Index;
}

void VPEDelayImportDirectoryEntryRef::moveNext() {
  ++Index;
}

vpe_imported_symbol_iterator
VPEDelayImportDirectoryEntryRef::imported_symbol_begin() const {
  return importedSymbolBegin(Table[Index].DelayImportNameTable,
                             OwningObject);
}

vpe_imported_symbol_iterator
VPEDelayImportDirectoryEntryRef::imported_symbol_end() const {
  return importedSymbolEnd(Table[Index].DelayImportNameTable,
                           OwningObject);
}

iterator_range<vpe_imported_symbol_iterator>
VPEDelayImportDirectoryEntryRef::imported_symbols() const {
  return make_range(imported_symbol_begin(), imported_symbol_end());
}

Error VPEDelayImportDirectoryEntryRef::getName(StringRef &Result) const {
  uintptr_t IntPtr = 0;
  if (Error E = OwningObject->getRvaPtr(Table[Index].Name, IntPtr))
    return E;
  Result = StringRef(reinterpret_cast<const char *>(IntPtr));
  return Error::success();
}

Error VPEDelayImportDirectoryEntryRef::getDelayImportTable(
  const vpe_delay_import_directory_table_entry *&Result) const {
  Result = &Table[Index];
  return Error::success();
}

Error VPEDelayImportDirectoryEntryRef::getImportAddress(int AddrIndex,
                                                        uint64_t &Result) const {
  uint32_t RVA = Table[Index].DelayImportAddressTable +
      AddrIndex * (OwningObject->is64() ? 8 : 4);
  uintptr_t IntPtr = 0;
  if (Error E = OwningObject->getRvaPtr(RVA, IntPtr))
    return E;
  if (OwningObject->is64())
    Result = *reinterpret_cast<const ulittle64_t *>(IntPtr);
  else
    Result = *reinterpret_cast<const ulittle32_t *>(IntPtr);
  return Error::success();
}

bool VPEExportDirectoryEntryRef::
operator==(const VPEExportDirectoryEntryRef &Other) const {
  return ExportTable == Other.ExportTable && Index == Other.Index;
}

void VPEExportDirectoryEntryRef::moveNext() {
  ++Index;
}

// Returns the name of the current export symbol. If the symbol is exported only
// by ordinal, the empty string is set as a result.
Error VPEExportDirectoryEntryRef::getDllName(StringRef &Result) const {
  uintptr_t IntPtr = 0;
  if (Error E = OwningObject->getRvaPtr(ExportTable->NameRVA, IntPtr))
    return E;
  Result = StringRef(reinterpret_cast<const char *>(IntPtr));
  return Error::success();
}

// Returns the starting ordinal number.
Error VPEExportDirectoryEntryRef::getOrdinalBase(uint32_t &Result) const {
  Result = ExportTable->OrdinalBase;
  return Error::success();
}

// Returns the export ordinal of the current export symbol.
Error VPEExportDirectoryEntryRef::getOrdinal(uint32_t &Result) const {
  Result = ExportTable->OrdinalBase + Index;
  return Error::success();
}

// Returns the address of the current export symbol.
Error VPEExportDirectoryEntryRef::getExportRVA(uint32_t &Result) const {
  uintptr_t IntPtr = 0;
  if (Error EC =
          OwningObject->getRvaPtr(ExportTable->ExportAddressTableRVA, IntPtr))
    return EC;
  const vpe_export_address_table_entry *entry =
      reinterpret_cast<const vpe_export_address_table_entry *>(IntPtr);
  Result = entry[Index].ExportRVA;
  return Error::success();
}

// Returns the name of the current export symbol. If the symbol is exported only
// by ordinal, the empty string is set as a result.
Error
VPEExportDirectoryEntryRef::getSymbolName(StringRef &Result) const {
  uintptr_t IntPtr = 0;
  if (Error EC =
          OwningObject->getRvaPtr(ExportTable->OrdinalTableRVA, IntPtr))
    return EC;
  const ulittle16_t *Start = reinterpret_cast<const ulittle16_t *>(IntPtr);

  uint32_t NumEntries = ExportTable->NumberOfNamePointers;
  int Offset = 0;
  for (const ulittle16_t *I = Start, *E = Start + NumEntries;
       I < E; ++I, ++Offset) {
    if (*I != Index)
      continue;
    if (Error EC =
            OwningObject->getRvaPtr(ExportTable->NamePointerRVA, IntPtr))
      return EC;
    const ulittle32_t *NamePtr = reinterpret_cast<const ulittle32_t *>(IntPtr);
    if (Error EC = OwningObject->getRvaPtr(NamePtr[Offset], IntPtr))
      return EC;
    Result = StringRef(reinterpret_cast<const char *>(IntPtr));
    return Error::success();
  }
  Result = "";
  return Error::success();
}

Error VPEExportDirectoryEntryRef::isForwarder(bool &Result) const {
  const vpe_data_directory *DataEntry =
      OwningObject->getDataDirectory(COFF::EXPORT_TABLE);
  if (!DataEntry)
    return errorCodeToError(object_error::parse_failed);
  uint32_t RVA;
  if (auto EC = getExportRVA(RVA))
    return EC;
  uint32_t Begin = DataEntry->RelativeVirtualAddress;
  uint32_t End = DataEntry->RelativeVirtualAddress + DataEntry->Size;
  Result = (Begin <= RVA && RVA < End);
  return Error::success();
}

Error VPEExportDirectoryEntryRef::getForwardTo(StringRef &Result) const {
  uint32_t RVA;
  if (auto EC = getExportRVA(RVA))
    return EC;
  uintptr_t IntPtr = 0;
  if (auto EC = OwningObject->getRvaPtr(RVA, IntPtr))
    return EC;
  Result = StringRef(reinterpret_cast<const char *>(IntPtr));
  return Error::success();
}

bool VPEImportedSymbolRef::operator==(const VPEImportedSymbolRef &Other) const {
  return Entry32 == Other.Entry32 && Entry64 == Other.Entry64
      && Index == Other.Index;
}

void VPEImportedSymbolRef::moveNext() {
  ++Index;
}

Error VPEImportedSymbolRef::getSymbolName(StringRef &Result) const {
  uint32_t RVA;
  if (Entry32) {
    // If a symbol is imported only by ordinal, it has no name.
    if (Entry32[Index].isOrdinal())
      return Error::success();
    RVA = Entry32[Index].getHintNameRVA();
  } else {
    if (Entry64[Index].isOrdinal())
      return Error::success();
    RVA = Entry64[Index].getHintNameRVA();
  }
  uintptr_t IntPtr = 0;
  if (Error EC = OwningObject->getRvaPtr(RVA, IntPtr))
    return EC;
  // +2 because the first two bytes is hint.
  Result = StringRef(reinterpret_cast<const char *>(IntPtr + 2));
  return Error::success();
}

Error VPEImportedSymbolRef::isOrdinal(bool &Result) const {
  if (Entry32)
    Result = Entry32[Index].isOrdinal();
  else
    Result = Entry64[Index].isOrdinal();
  return Error::success();
}

Error VPEImportedSymbolRef::getHintNameRVA(uint32_t &Result) const {
  if (Entry32)
    Result = Entry32[Index].getHintNameRVA();
  else
    Result = Entry64[Index].getHintNameRVA();
  return Error::success();
}

Error VPEImportedSymbolRef::getOrdinal(uint16_t &Result) const {
  uint32_t RVA;
  if (Entry32) {
    if (Entry32[Index].isOrdinal()) {
      Result = Entry32[Index].getOrdinal();
      return Error::success();
    }
    RVA = Entry32[Index].getHintNameRVA();
  } else {
    if (Entry64[Index].isOrdinal()) {
      Result = Entry64[Index].getOrdinal();
      return Error::success();
    }
    RVA = Entry64[Index].getHintNameRVA();
  }
  uintptr_t IntPtr = 0;
  if (Error EC = OwningObject->getRvaPtr(RVA, IntPtr))
    return EC;
  Result = *reinterpret_cast<const ulittle16_t *>(IntPtr);
  return Error::success();
}

Expected<std::unique_ptr<VPEObjectFile>>
ObjectFile::createVPEObjectFile(MemoryBufferRef Object) {
  return VPEObjectFile::create(Object);
}

bool VPEBaseRelocRef::operator==(const VPEBaseRelocRef &Other) const {
  return Header == Other.Header && Index == Other.Index;
}

void VPEBaseRelocRef::moveNext() {
  // Header->BlockSize is the size of the current block, including the
  // size of the header itself.
  uint32_t Size = sizeof(*Header) +
      sizeof(vpe_base_reloc_block_entry) * (Index + 1);
  if (Size == Header->BlockSize) {
    // .reloc contains a list of base relocation blocks. Each block
    // consists of the header followed by entries. The header contains
    // how many entories will follow. When we reach the end of the
    // current block, proceed to the next block.
    Header = reinterpret_cast<const vpe_base_reloc_block_header *>(
        reinterpret_cast<const uint8_t *>(Header) + Size);
    Index = 0;
  } else {
    ++Index;
  }
}

Error VPEBaseRelocRef::getType(uint8_t &Type) const {
  auto *Entry = reinterpret_cast<const vpe_base_reloc_block_entry *>(Header + 1);
  Type = Entry[Index].getType();
  return Error::success();
}

Error VPEBaseRelocRef::getRVA(uint32_t &Result) const {
  auto *Entry = reinterpret_cast<const vpe_base_reloc_block_entry *>(Header + 1);
  Result = Header->PageRVA + Entry[Index].getOffset();
  return Error::success();
}

#define RETURN_IF_ERROR(E)                                                     \
  if (E)                                                                       \
    return E;

Expected<ArrayRef<UTF16>>
VPEResourceSectionRef::getDirStringAtOffset(uint32_t Offset) {
  BinaryStreamReader Reader = BinaryStreamReader(BBS);
  Reader.setOffset(Offset);
  uint16_t Length;
  RETURN_IF_ERROR(Reader.readInteger(Length));
  ArrayRef<UTF16> RawDirString;
  RETURN_IF_ERROR(Reader.readArray(RawDirString, Length));
  return RawDirString;
}

Expected<ArrayRef<UTF16>>
VPEResourceSectionRef::getEntryNameString(const vpe_resource_dir_entry &Entry) {
  return getDirStringAtOffset(Entry.Identifier.getNameOffset());
}

Expected<const vpe_resource_dir_table &>
VPEResourceSectionRef::getTableAtOffset(uint32_t Offset) {
  const vpe_resource_dir_table *Table = nullptr;

  BinaryStreamReader Reader(BBS);
  Reader.setOffset(Offset);
  RETURN_IF_ERROR(Reader.readObject(Table));
  assert(Table != nullptr);
  return *Table;
}

Expected<const vpe_resource_dir_table &>
VPEResourceSectionRef::getEntrySubDir(const vpe_resource_dir_entry &Entry) {
  return getTableAtOffset(Entry.Offset.value());
}

Expected<const vpe_resource_dir_table &> VPEResourceSectionRef::getBaseTable() {
  return getTableAtOffset(0);
}
