//===- VPEYAML.cpp - VPE YAMLIO implementation ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines classes for handling the YAML representation of VPE.
//
//===----------------------------------------------------------------------===//

#include "llvm/ObjectYAML/VPEYAML.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/YAMLTraits.h"
#include <cstdint>
#include <cstring>

#define ECase(X) IO.enumCase(Value, #X, VPE::X);

namespace llvm {

namespace VPEYAML {

Section::Section() { memset(&Header, 0, sizeof(VPE::section)); }
Symbol::Symbol() { memset(&Header, 0, sizeof(VPE::symbol)); }
Object::Object() { memset(&Header, 0, sizeof(VPE::header)); }

} // end namespace VPEYAML

namespace yaml {

void ScalarEnumerationTraits<VPEYAML::COMDATType>::enumeration(
    IO &IO, VPEYAML::COMDATType &Value) {
  IO.enumCase(Value, "0", 0);
  ECase(IMAGE_COMDAT_SELECT_NODUPLICATES);
  ECase(IMAGE_COMDAT_SELECT_ANY);
  ECase(IMAGE_COMDAT_SELECT_SAME_SIZE);
  ECase(IMAGE_COMDAT_SELECT_EXACT_MATCH);
  ECase(IMAGE_COMDAT_SELECT_ASSOCIATIVE);
  ECase(IMAGE_COMDAT_SELECT_LARGEST);
  ECase(IMAGE_COMDAT_SELECT_NEWEST);
}

void
ScalarEnumerationTraits<VPEYAML::WeakExternalCharacteristics>::enumeration(
    IO &IO, VPEYAML::WeakExternalCharacteristics &Value) {
  IO.enumCase(Value, "0", 0);
  ECase(IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY);
  ECase(IMAGE_WEAK_EXTERN_SEARCH_LIBRARY);
  ECase(IMAGE_WEAK_EXTERN_SEARCH_ALIAS);
  ECase(IMAGE_WEAK_EXTERN_ANTI_DEPENDENCY);
}

void ScalarEnumerationTraits<VPEYAML::AuxSymbolType>::enumeration(
    IO &IO, VPEYAML::AuxSymbolType &Value) {
  ECase(IMAGE_AUX_SYMBOL_TYPE_TOKEN_DEF);
}

void ScalarEnumerationTraits<VPE::MachineTypes>::enumeration(
    IO &IO, VPE::MachineTypes &Value) {
  ECase(IMAGE_FILE_MACHINE_UNKNOWN);
  ECase(IMAGE_FILE_MACHINE_AM33);
  ECase(IMAGE_FILE_MACHINE_AMD64);
  ECase(IMAGE_FILE_MACHINE_ARM);
  ECase(IMAGE_FILE_MACHINE_ARMNT);
  ECase(IMAGE_FILE_MACHINE_ARM64);
  ECase(IMAGE_FILE_MACHINE_ARM64EC);
  ECase(IMAGE_FILE_MACHINE_EBC);
  ECase(IMAGE_FILE_MACHINE_I386);
  ECase(IMAGE_FILE_MACHINE_IA64);
  ECase(IMAGE_FILE_MACHINE_M32R);
  ECase(IMAGE_FILE_MACHINE_MIPS16);
  ECase(IMAGE_FILE_MACHINE_MIPSFPU);
  ECase(IMAGE_FILE_MACHINE_MIPSFPU16);
  ECase(IMAGE_FILE_MACHINE_POWERPC);
  ECase(IMAGE_FILE_MACHINE_POWERPCFP);
  ECase(IMAGE_FILE_MACHINE_R4000);
  ECase(IMAGE_FILE_MACHINE_RISCV32);
  ECase(IMAGE_FILE_MACHINE_RISCV64);
  ECase(IMAGE_FILE_MACHINE_RISCV128);
  ECase(IMAGE_FILE_MACHINE_SH3);
  ECase(IMAGE_FILE_MACHINE_SH3DSP);
  ECase(IMAGE_FILE_MACHINE_SH4);
  ECase(IMAGE_FILE_MACHINE_SH5);
  ECase(IMAGE_FILE_MACHINE_THUMB);
  ECase(IMAGE_FILE_MACHINE_WCEMIPSV2);
}

void ScalarEnumerationTraits<VPE::SymbolBaseType>::enumeration(
    IO &IO, VPE::SymbolBaseType &Value) {
  ECase(IMAGE_SYM_TYPE_NULL);
  ECase(IMAGE_SYM_TYPE_VOID);
  ECase(IMAGE_SYM_TYPE_CHAR);
  ECase(IMAGE_SYM_TYPE_SHORT);
  ECase(IMAGE_SYM_TYPE_INT);
  ECase(IMAGE_SYM_TYPE_LONG);
  ECase(IMAGE_SYM_TYPE_FLOAT);
  ECase(IMAGE_SYM_TYPE_DOUBLE);
  ECase(IMAGE_SYM_TYPE_STRUCT);
  ECase(IMAGE_SYM_TYPE_UNION);
  ECase(IMAGE_SYM_TYPE_ENUM);
  ECase(IMAGE_SYM_TYPE_MOE);
  ECase(IMAGE_SYM_TYPE_BYTE);
  ECase(IMAGE_SYM_TYPE_WORD);
  ECase(IMAGE_SYM_TYPE_UINT);
  ECase(IMAGE_SYM_TYPE_DWORD);
}

void ScalarEnumerationTraits<VPE::SymbolStorageClass>::enumeration(
    IO &IO, VPE::SymbolStorageClass &Value) {
  ECase(IMAGE_SYM_CLASS_END_OF_FUNCTION);
  ECase(IMAGE_SYM_CLASS_NULL);
  ECase(IMAGE_SYM_CLASS_AUTOMATIC);
  ECase(IMAGE_SYM_CLASS_EXTERNAL);
  ECase(IMAGE_SYM_CLASS_STATIC);
  ECase(IMAGE_SYM_CLASS_REGISTER);
  ECase(IMAGE_SYM_CLASS_EXTERNAL_DEF);
  ECase(IMAGE_SYM_CLASS_LABEL);
  ECase(IMAGE_SYM_CLASS_UNDEFINED_LABEL);
  ECase(IMAGE_SYM_CLASS_MEMBER_OF_STRUCT);
  ECase(IMAGE_SYM_CLASS_ARGUMENT);
  ECase(IMAGE_SYM_CLASS_STRUCT_TAG);
  ECase(IMAGE_SYM_CLASS_MEMBER_OF_UNION);
  ECase(IMAGE_SYM_CLASS_UNION_TAG);
  ECase(IMAGE_SYM_CLASS_TYPE_DEFINITION);
  ECase(IMAGE_SYM_CLASS_UNDEFINED_STATIC);
  ECase(IMAGE_SYM_CLASS_ENUM_TAG);
  ECase(IMAGE_SYM_CLASS_MEMBER_OF_ENUM);
  ECase(IMAGE_SYM_CLASS_REGISTER_PARAM);
  ECase(IMAGE_SYM_CLASS_BIT_FIELD);
  ECase(IMAGE_SYM_CLASS_BLOCK);
  ECase(IMAGE_SYM_CLASS_FUNCTION);
  ECase(IMAGE_SYM_CLASS_END_OF_STRUCT);
  ECase(IMAGE_SYM_CLASS_FILE);
  ECase(IMAGE_SYM_CLASS_SECTION);
  ECase(IMAGE_SYM_CLASS_WEAK_EXTERNAL);
  ECase(IMAGE_SYM_CLASS_CLR_TOKEN);
}

void ScalarEnumerationTraits<VPE::SymbolComplexType>::enumeration(
    IO &IO, VPE::SymbolComplexType &Value) {
  ECase(IMAGE_SYM_DTYPE_NULL);
  ECase(IMAGE_SYM_DTYPE_POINTER);
  ECase(IMAGE_SYM_DTYPE_FUNCTION);
  ECase(IMAGE_SYM_DTYPE_ARRAY);
}

void ScalarEnumerationTraits<VPE::RelocationTypeI386>::enumeration(
    IO &IO, VPE::RelocationTypeI386 &Value) {
  ECase(IMAGE_REL_I386_ABSOLUTE);
  ECase(IMAGE_REL_I386_DIR16);
  ECase(IMAGE_REL_I386_REL16);
  ECase(IMAGE_REL_I386_DIR32);
  ECase(IMAGE_REL_I386_DIR32NB);
  ECase(IMAGE_REL_I386_SEG12);
  ECase(IMAGE_REL_I386_SECTION);
  ECase(IMAGE_REL_I386_SECREL);
  ECase(IMAGE_REL_I386_TOKEN);
  ECase(IMAGE_REL_I386_SECREL7);
  ECase(IMAGE_REL_I386_REL32);
}

void ScalarEnumerationTraits<VPE::RelocationTypeAMD64>::enumeration(
    IO &IO, VPE::RelocationTypeAMD64 &Value) {
  ECase(IMAGE_REL_AMD64_ABSOLUTE);
  ECase(IMAGE_REL_AMD64_ADDR64);
  ECase(IMAGE_REL_AMD64_ADDR32);
  ECase(IMAGE_REL_AMD64_ADDR32NB);
  ECase(IMAGE_REL_AMD64_REL32);
  ECase(IMAGE_REL_AMD64_REL32_1);
  ECase(IMAGE_REL_AMD64_REL32_2);
  ECase(IMAGE_REL_AMD64_REL32_3);
  ECase(IMAGE_REL_AMD64_REL32_4);
  ECase(IMAGE_REL_AMD64_REL32_5);
  ECase(IMAGE_REL_AMD64_SECTION);
  ECase(IMAGE_REL_AMD64_SECREL);
  ECase(IMAGE_REL_AMD64_SECREL7);
  ECase(IMAGE_REL_AMD64_TOKEN);
  ECase(IMAGE_REL_AMD64_SREL32);
  ECase(IMAGE_REL_AMD64_PAIR);
  ECase(IMAGE_REL_AMD64_SSPAN32);
}

void ScalarEnumerationTraits<VPE::RelocationTypesARM>::enumeration(
    IO &IO, VPE::RelocationTypesARM &Value) {
  ECase(IMAGE_REL_ARM_ABSOLUTE);
  ECase(IMAGE_REL_ARM_ADDR32);
  ECase(IMAGE_REL_ARM_ADDR32NB);
  ECase(IMAGE_REL_ARM_BRANCH24);
  ECase(IMAGE_REL_ARM_BRANCH11);
  ECase(IMAGE_REL_ARM_TOKEN);
  ECase(IMAGE_REL_ARM_BLX24);
  ECase(IMAGE_REL_ARM_BLX11);
  ECase(IMAGE_REL_ARM_REL32);
  ECase(IMAGE_REL_ARM_SECTION);
  ECase(IMAGE_REL_ARM_SECREL);
  ECase(IMAGE_REL_ARM_MOV32A);
  ECase(IMAGE_REL_ARM_MOV32T);
  ECase(IMAGE_REL_ARM_BRANCH20T);
  ECase(IMAGE_REL_ARM_BRANCH24T);
  ECase(IMAGE_REL_ARM_BLX23T);
  ECase(IMAGE_REL_ARM_PAIR);
}

void ScalarEnumerationTraits<VPE::RelocationTypesARM64>::enumeration(
    IO &IO, VPE::RelocationTypesARM64 &Value) {
  ECase(IMAGE_REL_ARM64_ABSOLUTE);
  ECase(IMAGE_REL_ARM64_ADDR32);
  ECase(IMAGE_REL_ARM64_ADDR32NB);
  ECase(IMAGE_REL_ARM64_BRANCH26);
  ECase(IMAGE_REL_ARM64_PAGEBASE_REL21);
  ECase(IMAGE_REL_ARM64_REL21);
  ECase(IMAGE_REL_ARM64_PAGEOFFSET_12A);
  ECase(IMAGE_REL_ARM64_PAGEOFFSET_12L);
  ECase(IMAGE_REL_ARM64_SECREL);
  ECase(IMAGE_REL_ARM64_SECREL_LOW12A);
  ECase(IMAGE_REL_ARM64_SECREL_HIGH12A);
  ECase(IMAGE_REL_ARM64_SECREL_LOW12L);
  ECase(IMAGE_REL_ARM64_TOKEN);
  ECase(IMAGE_REL_ARM64_SECTION);
  ECase(IMAGE_REL_ARM64_ADDR64);
  ECase(IMAGE_REL_ARM64_BRANCH19);
  ECase(IMAGE_REL_ARM64_BRANCH14);
  ECase(IMAGE_REL_ARM64_REL32);
}
#undef ECase

#define BCase(X) IO.bitSetCase(Value, #X, VPE::X);
void ScalarBitSetTraits<VPE::Characteristics>::bitset(
    IO &IO, VPE::Characteristics &Value) {
  BCase(IMAGE_FILE_RELOCS_STRIPPED);
  BCase(IMAGE_FILE_EXECUTABLE_IMAGE);
  BCase(IMAGE_FILE_LINE_NUMS_STRIPPED);
  BCase(IMAGE_FILE_LOCAL_SYMS_STRIPPED);
  BCase(IMAGE_FILE_AGGRESSIVE_WS_TRIM);
  BCase(IMAGE_FILE_LARGE_ADDRESS_AWARE);
  BCase(IMAGE_FILE_BYTES_REVERSED_LO);
  BCase(IMAGE_FILE_32BIT_MACHINE);
  BCase(IMAGE_FILE_DEBUG_STRIPPED);
  BCase(IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP);
  BCase(IMAGE_FILE_NET_RUN_FROM_SWAP);
  BCase(IMAGE_FILE_SYSTEM);
  BCase(IMAGE_FILE_DLL);
  BCase(IMAGE_FILE_UP_SYSTEM_ONLY);
  BCase(IMAGE_FILE_BYTES_REVERSED_HI);
}

void ScalarBitSetTraits<VPE::SectionCharacteristics>::bitset(
    IO &IO, VPE::SectionCharacteristics &Value) {
  BCase(IMAGE_SCN_TYPE_NOLOAD);
  BCase(IMAGE_SCN_TYPE_NO_PAD);
  BCase(IMAGE_SCN_CNT_CODE);
  BCase(IMAGE_SCN_CNT_INITIALIZED_DATA);
  BCase(IMAGE_SCN_CNT_UNINITIALIZED_DATA);
  BCase(IMAGE_SCN_LNK_OTHER);
  BCase(IMAGE_SCN_LNK_INFO);
  BCase(IMAGE_SCN_LNK_REMOVE);
  BCase(IMAGE_SCN_LNK_COMDAT);
  BCase(IMAGE_SCN_GPREL);
  BCase(IMAGE_SCN_MEM_PURGEABLE);
  BCase(IMAGE_SCN_MEM_16BIT);
  BCase(IMAGE_SCN_MEM_LOCKED);
  BCase(IMAGE_SCN_MEM_PRELOAD);
  BCase(IMAGE_SCN_LNK_NRELOC_OVFL);
  BCase(IMAGE_SCN_MEM_DISCARDABLE);
  BCase(IMAGE_SCN_MEM_NOT_CACHED);
  BCase(IMAGE_SCN_MEM_NOT_PAGED);
  BCase(IMAGE_SCN_MEM_SHARED);
  BCase(IMAGE_SCN_MEM_EXECUTE);
  BCase(IMAGE_SCN_MEM_READ);
  BCase(IMAGE_SCN_MEM_WRITE);
}

void ScalarBitSetTraits<VPE::DLLCharacteristics>::bitset(
    IO &IO, VPE::DLLCharacteristics &Value) {
  BCase(IMAGE_DLL_CHARACTERISTICS_HIGH_ENTROPY_VA);
  BCase(IMAGE_DLL_CHARACTERISTICS_DYNAMIC_BASE);
  BCase(IMAGE_DLL_CHARACTERISTICS_FORCE_INTEGRITY);
  BCase(IMAGE_DLL_CHARACTERISTICS_NX_COMPAT);
  BCase(IMAGE_DLL_CHARACTERISTICS_NO_ISOLATION);
  BCase(IMAGE_DLL_CHARACTERISTICS_NO_SEH);
  BCase(IMAGE_DLL_CHARACTERISTICS_NO_BIND);
  BCase(IMAGE_DLL_CHARACTERISTICS_APPCONTAINER);
  BCase(IMAGE_DLL_CHARACTERISTICS_WDM_DRIVER);
  BCase(IMAGE_DLL_CHARACTERISTICS_GUARD_CF);
  BCase(IMAGE_DLL_CHARACTERISTICS_TERMINAL_SERVER_AWARE);
}
#undef BCase

namespace {

struct NSectionSelectionType {
  NSectionSelectionType(IO &)
      : SelectionType(VPEYAML::COMDATType(0)) {}
  NSectionSelectionType(IO &, uint8_t C)
      : SelectionType(VPEYAML::COMDATType(C)) {}

  uint8_t denormalize(IO &) { return SelectionType; }

  VPEYAML::COMDATType SelectionType;
};

struct NWeakExternalCharacteristics {
  NWeakExternalCharacteristics(IO &)
      : Characteristics(VPEYAML::WeakExternalCharacteristics(0)) {}
  NWeakExternalCharacteristics(IO &, uint32_t C)
      : Characteristics(VPEYAML::WeakExternalCharacteristics(C)) {}

  uint32_t denormalize(IO &) { return Characteristics; }

  VPEYAML::WeakExternalCharacteristics Characteristics;
};

struct NSectionCharacteristics {
  NSectionCharacteristics(IO &)
      : Characteristics(VPE::SectionCharacteristics(0)) {}
  NSectionCharacteristics(IO &, uint32_t C)
      : Characteristics(VPE::SectionCharacteristics(C)) {}

  uint32_t denormalize(IO &) { return Characteristics; }

  VPE::SectionCharacteristics Characteristics;
};

struct NAuxTokenType {
  NAuxTokenType(IO &)
      : AuxType(VPEYAML::AuxSymbolType(0)) {}
  NAuxTokenType(IO &, uint8_t C)
      : AuxType(VPEYAML::AuxSymbolType(C)) {}

  uint32_t denormalize(IO &) { return AuxType; }

  VPEYAML::AuxSymbolType AuxType;
};

struct NStorageClass {
  NStorageClass(IO &) : StorageClass(VPE::SymbolStorageClass(0)) {}
  NStorageClass(IO &, uint8_t S) : StorageClass(VPE::SymbolStorageClass(S)) {}

  uint8_t denormalize(IO &) { return StorageClass; }

  VPE::SymbolStorageClass StorageClass;
};

struct NMachine {
  NMachine(IO &) : Machine(VPE::MachineTypes(0)) {}
  NMachine(IO &, uint16_t M) : Machine(VPE::MachineTypes(M)) {}

  uint16_t denormalize(IO &) { return Machine; }

  VPE::MachineTypes Machine;
};

struct NHeaderCharacteristics {
  NHeaderCharacteristics(IO &) : Characteristics(VPE::Characteristics(0)) {}
  NHeaderCharacteristics(IO &, uint16_t C)
      : Characteristics(VPE::Characteristics(C)) {}

  uint16_t denormalize(IO &) { return Characteristics; }

  VPE::Characteristics Characteristics;
};

template <typename RelocType>
struct NType {
  NType(IO &) : Type(RelocType(0)) {}
  NType(IO &, uint16_t T) : Type(RelocType(T)) {}

  uint16_t denormalize(IO &) { return Type; }

  RelocType Type;
};

struct NDLLCharacteristics {
  NDLLCharacteristics(IO &) : Characteristics(VPE::DLLCharacteristics(0)) {}
  NDLLCharacteristics(IO &, uint16_t C)
      : Characteristics(VPE::DLLCharacteristics(C)) {}

  uint16_t denormalize(IO &) { return Characteristics; }

  VPE::DLLCharacteristics Characteristics;
};

} // end anonymous namespace

void MappingTraits<VPEYAML::Relocation>::mapping(IO &IO,
                                                  VPEYAML::Relocation &Rel) {
  IO.mapRequired("VirtualAddress", Rel.VirtualAddress);
  IO.mapOptional("SymbolName", Rel.SymbolName, StringRef());
  IO.mapOptional("SymbolTableIndex", Rel.SymbolTableIndex);

  VPE::header &H = *static_cast<VPE::header *>(IO.getContext());
  if (H.Machine == VPE::IMAGE_FILE_MACHINE_I386) {
    MappingNormalization<NType<VPE::RelocationTypeI386>, uint16_t> NT(
        IO, Rel.Type);
    IO.mapRequired("Type", NT->Type);
  } else if (H.Machine == VPE::IMAGE_FILE_MACHINE_AMD64) {
    MappingNormalization<NType<VPE::RelocationTypeAMD64>, uint16_t> NT(
        IO, Rel.Type);
    IO.mapRequired("Type", NT->Type);
  } else if (H.Machine == VPE::IMAGE_FILE_MACHINE_ARMNT) {
    MappingNormalization<NType<VPE::RelocationTypesARM>, uint16_t> NT(
        IO, Rel.Type);
    IO.mapRequired("Type", NT->Type);
  } else if (H.Machine == VPE::IMAGE_FILE_MACHINE_ARM64 ||
             H.Machine == VPE::IMAGE_FILE_MACHINE_ARM64EC) {
    MappingNormalization<NType<VPE::RelocationTypesARM64>, uint16_t> NT(
        IO, Rel.Type);
    IO.mapRequired("Type", NT->Type);
  } else {
    IO.mapRequired("Type", Rel.Type);
  }
}

void MappingTraits<VPE::DataDirectory>::mapping(IO &IO,
                                                 VPE::DataDirectory &DD) {
  IO.mapRequired("RelativeVirtualAddress", DD.RelativeVirtualAddress);
  IO.mapRequired("Size", DD.Size);
}

void MappingTraits<VPEYAML::PEHeader>::mapping(IO &IO,
                                                VPEYAML::PEHeader &PH) {
  MappingNormalization<NDLLCharacteristics, uint16_t> NDC(
      IO, PH.Header.DllCharacteristics);

  IO.mapOptional("AddressOfEntryPoint", PH.Header.AddressOfEntryPoint);
  IO.mapOptional("ImageBase", PH.Header.ImageBase);
  IO.mapOptional("SectionAlignment", PH.Header.SectionAlignment, 1);
  IO.mapOptional("FileAlignment", PH.Header.FileAlignment, 1);
  IO.mapOptional("MajorOperatingSystemVersion",
                 PH.Header.MajorOperatingSystemVersion);
  IO.mapOptional("MinorOperatingSystemVersion",
                 PH.Header.MinorOperatingSystemVersion);
  IO.mapOptional("MajorImageVersion", PH.Header.MajorImageVersion);
  IO.mapOptional("MinorImageVersion", PH.Header.MinorImageVersion);
  IO.mapOptional("MajorSubsystemVersion", PH.Header.MajorSubsystemVersion);
  IO.mapOptional("MinorSubsystemVersion", PH.Header.MinorSubsystemVersion);
  IO.mapOptional("DLLCharacteristics", NDC->Characteristics);
  IO.mapOptional("SizeOfStackReserve", PH.Header.SizeOfStackReserve);
  IO.mapOptional("SizeOfStackCommit", PH.Header.SizeOfStackCommit);
  IO.mapOptional("SizeOfHeapReserve", PH.Header.SizeOfHeapReserve);
  IO.mapOptional("SizeOfHeapCommit", PH.Header.SizeOfHeapCommit);

  IO.mapOptional("NumberOfRvaAndSize", PH.Header.NumberOfRvaAndSizes,
                 VPE::NUM_DATA_DIRECTORIES + 1);
  IO.mapOptional("ExportTable", PH.DataDirectories[VPE::EXPORT_TABLE]);
  IO.mapOptional("ImportTable", PH.DataDirectories[VPE::IMPORT_TABLE]);
  IO.mapOptional("ResourceTable", PH.DataDirectories[VPE::RESOURCE_TABLE]);
  IO.mapOptional("ExceptionTable", PH.DataDirectories[VPE::EXCEPTION_TABLE]);
  IO.mapOptional("CertificateTable", PH.DataDirectories[VPE::CERTIFICATE_TABLE]);
  IO.mapOptional("BaseRelocationTable",
                 PH.DataDirectories[VPE::BASE_RELOCATION_TABLE]);
  IO.mapOptional("Debug", PH.DataDirectories[VPE::DEBUG_DIRECTORY]);
  IO.mapOptional("Architecture", PH.DataDirectories[VPE::ARCHITECTURE]);
  IO.mapOptional("GlobalPtr", PH.DataDirectories[VPE::GLOBAL_PTR]);
  IO.mapOptional("TlsTable", PH.DataDirectories[VPE::TLS_TABLE]);
  IO.mapOptional("LoadConfigTable",
                 PH.DataDirectories[VPE::LOAD_CONFIG_TABLE]);
  IO.mapOptional("BoundImport", PH.DataDirectories[VPE::BOUND_IMPORT]);
  IO.mapOptional("IAT", PH.DataDirectories[VPE::IAT]);
  IO.mapOptional("DelayImportDescriptor",
                 PH.DataDirectories[VPE::DELAY_IMPORT_DESCRIPTOR]);
  IO.mapOptional("ClrRuntimeHeader",
                 PH.DataDirectories[VPE::CLR_RUNTIME_HEADER]);
}

void MappingTraits<VPE::header>::mapping(IO &IO, VPE::header &H) {
  MappingNormalization<NMachine, uint16_t> NM(IO, H.Machine);
  MappingNormalization<NHeaderCharacteristics, uint16_t> NC(IO,
                                                            H.Characteristics);

  IO.mapRequired("Machine", NM->Machine);
  IO.mapOptional("Characteristics", NC->Characteristics);
  IO.setContext(static_cast<void *>(&H));
}

void MappingTraits<VPE::AuxiliaryFunctionDefinition>::mapping(
    IO &IO, VPE::AuxiliaryFunctionDefinition &AFD) {
  IO.mapRequired("TagIndex", AFD.TagIndex);
  IO.mapRequired("TotalSize", AFD.TotalSize);
  IO.mapRequired("PointerToLinenumber", AFD.PointerToLinenumber);
  IO.mapRequired("PointerToNextFunction", AFD.PointerToNextFunction);
}

void MappingTraits<VPE::AuxiliarybfAndefSymbol>::mapping(
    IO &IO, VPE::AuxiliarybfAndefSymbol &AAS) {
  IO.mapRequired("Linenumber", AAS.Linenumber);
  IO.mapRequired("PointerToNextFunction", AAS.PointerToNextFunction);
}

void MappingTraits<VPE::AuxiliaryWeakExternal>::mapping(
    IO &IO, VPE::AuxiliaryWeakExternal &AWE) {
  MappingNormalization<NWeakExternalCharacteristics, uint32_t> NWEC(
      IO, AWE.Characteristics);
  IO.mapRequired("TagIndex", AWE.TagIndex);
  IO.mapRequired("Characteristics", NWEC->Characteristics);
}

void MappingTraits<VPE::AuxiliarySectionDefinition>::mapping(
    IO &IO, VPE::AuxiliarySectionDefinition &ASD) {
  MappingNormalization<NSectionSelectionType, uint8_t> NSST(
      IO, ASD.Selection);

  IO.mapRequired("Length", ASD.Length);
  IO.mapRequired("NumberOfRelocations", ASD.NumberOfRelocations);
  IO.mapRequired("NumberOfLinenumbers", ASD.NumberOfLinenumbers);
  IO.mapRequired("CheckSum", ASD.CheckSum);
  IO.mapRequired("Number", ASD.Number);
  IO.mapOptional("Selection", NSST->SelectionType, VPEYAML::COMDATType(0));
}

void MappingTraits<VPE::AuxiliaryCLRToken>::mapping(
    IO &IO, VPE::AuxiliaryCLRToken &ACT) {
  MappingNormalization<NAuxTokenType, uint8_t> NATT(IO, ACT.AuxType);
  IO.mapRequired("AuxType", NATT->AuxType);
  IO.mapRequired("SymbolTableIndex", ACT.SymbolTableIndex);
}

void MappingTraits<VPEYAML::Symbol>::mapping(IO &IO, VPEYAML::Symbol &S) {
  MappingNormalization<NStorageClass, uint8_t> NS(IO, S.Header.StorageClass);

  IO.mapRequired("Name", S.Name);
  IO.mapRequired("Value", S.Header.Value);
  IO.mapRequired("SectionNumber", S.Header.SectionNumber);
  IO.mapRequired("SimpleType", S.SimpleType);
  IO.mapRequired("ComplexType", S.ComplexType);
  IO.mapRequired("StorageClass", NS->StorageClass);
  IO.mapOptional("FunctionDefinition", S.FunctionDefinition);
  IO.mapOptional("bfAndefSymbol", S.bfAndefSymbol);
  IO.mapOptional("WeakExternal", S.WeakExternal);
  IO.mapOptional("File", S.File, StringRef());
  IO.mapOptional("SectionDefinition", S.SectionDefinition);
  IO.mapOptional("CLRToken", S.CLRToken);
}

void MappingTraits<VPEYAML::Section>::mapping(IO &IO, VPEYAML::Section &Sec) {
  MappingNormalization<NSectionCharacteristics, uint32_t> NC(
      IO, Sec.Header.Characteristics);
  IO.mapRequired("Name", Sec.Name);
  IO.mapRequired("Characteristics", NC->Characteristics);
  IO.mapOptional("VirtualAddress", Sec.Header.VirtualAddress, 0U);
  IO.mapOptional("VirtualSize", Sec.Header.VirtualSize, 0U);
  IO.mapOptional("Alignment", Sec.Alignment, 0U);

  // If this is a .debug$S .debug$T .debug$P, or .debug$H section parse the
  // semantic representation of the symbols/types.  If it is any other kind
  // of section, just deal in raw bytes.
  IO.mapOptional("SectionData", Sec.SectionData);
  if (Sec.Name == ".debug$S")
    IO.mapOptional("Subsections", Sec.DebugS);
  else if (Sec.Name == ".debug$T")
    IO.mapOptional("Types", Sec.DebugT);
  else if (Sec.Name == ".debug$P")
    IO.mapOptional("PrecompTypes", Sec.DebugP);
  else if (Sec.Name == ".debug$H")
    IO.mapOptional("GlobalHashes", Sec.DebugH);

  // Uninitialized sections, such as .bss, typically have no data, but the size
  // is carried in SizeOfRawData, even though PointerToRawData is zero.
  if (Sec.SectionData.binary_size() == 0 &&
      NC->Characteristics & VPE::IMAGE_SCN_CNT_UNINITIALIZED_DATA)
    IO.mapOptional("SizeOfRawData", Sec.Header.SizeOfRawData);

  IO.mapOptional("Relocations", Sec.Relocations);
}

void MappingTraits<VPEYAML::Object>::mapping(IO &IO, VPEYAML::Object &Obj) {
  IO.mapTag("!VPE", true);
  IO.mapOptional("OptionalHeader", Obj.OptionalHeader);
  IO.mapRequired("header", Obj.Header);
  IO.mapRequired("sections", Obj.Sections);
  IO.mapRequired("symbols", Obj.Symbols);
}

} // end namespace yaml

} // end namespace llvm
