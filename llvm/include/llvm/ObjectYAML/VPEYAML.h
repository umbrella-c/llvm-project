//===- VPEYAML.h - COFF YAMLIO implementation ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares classes for handling the YAML representation of COFF.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_OBJECTYAML_VPEYAML_H
#define LLVM_OBJECTYAML_VPEYAML_H

#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/VPE.h"
#include "llvm/ObjectYAML/CodeViewYAMLDebugSections.h"
#include "llvm/ObjectYAML/CodeViewYAMLTypeHashing.h"
#include "llvm/ObjectYAML/CodeViewYAMLTypes.h"
#include "llvm/ObjectYAML/YAML.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace llvm {

namespace VPE {

inline Characteristics operator|(Characteristics a, Characteristics b) {
  uint32_t Ret = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
  return static_cast<Characteristics>(Ret);
}

inline SectionCharacteristics operator|(SectionCharacteristics a,
                                        SectionCharacteristics b) {
  uint32_t Ret = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
  return static_cast<SectionCharacteristics>(Ret);
}

inline DLLCharacteristics operator|(DLLCharacteristics a,
                                    DLLCharacteristics b) {
  uint16_t Ret = static_cast<uint16_t>(a) | static_cast<uint16_t>(b);
  return static_cast<DLLCharacteristics>(Ret);
}

} // end namespace VPE

// The structure of the yaml files is not an exact 1:1 match to COFF. In order
// to use yaml::IO, we use these structures which are closer to the source.
namespace VPEYAML {

LLVM_YAML_STRONG_TYPEDEF(uint8_t, COMDATType)
LLVM_YAML_STRONG_TYPEDEF(uint32_t, WeakExternalCharacteristics)
LLVM_YAML_STRONG_TYPEDEF(uint8_t, AuxSymbolType)

struct Relocation {
  uint32_t VirtualAddress;
  uint16_t Type;

  // Normally a Relocation can refer to the symbol via its name.
  // It can also use a direct symbol table index instead (with no name
  // specified), allowing disambiguating between multiple symbols with the
  // same name or crafting intentionally broken files for testing.
  StringRef SymbolName;
  std::optional<uint32_t> SymbolTableIndex;
};

struct Section {
  VPE::section Header;
  unsigned Alignment = 0;
  yaml::BinaryRef SectionData;
  std::vector<CodeViewYAML::YAMLDebugSubsection> DebugS;
  std::vector<CodeViewYAML::LeafRecord> DebugT;
  std::vector<CodeViewYAML::LeafRecord> DebugP;
  std::optional<CodeViewYAML::DebugHSection> DebugH;
  std::vector<Relocation> Relocations;
  StringRef Name;

  Section();
};

struct Symbol {
  VPE::symbol Header;
  VPE::SymbolBaseType SimpleType = VPE::IMAGE_SYM_TYPE_NULL;
  VPE::SymbolComplexType ComplexType = VPE::IMAGE_SYM_DTYPE_NULL;
  std::optional<VPE::AuxiliaryFunctionDefinition> FunctionDefinition;
  std::optional<VPE::AuxiliarybfAndefSymbol> bfAndefSymbol;
  std::optional<VPE::AuxiliaryWeakExternal> WeakExternal;
  StringRef File;
  std::optional<VPE::AuxiliarySectionDefinition> SectionDefinition;
  std::optional<VPE::AuxiliaryCLRToken> CLRToken;
  StringRef Name;

  Symbol();
};

struct PEHeader {
  VPE::PE32Header Header;
  std::optional<VPE::DataDirectory>
      DataDirectories[VPE::NUM_DATA_DIRECTORIES];
};

struct Object {
  std::optional<PEHeader> OptionalHeader;
  VPE::header Header;
  std::vector<Section> Sections;
  std::vector<Symbol> Symbols;

  Object();
};

} // end namespace VPEYAML

} // end namespace llvm

LLVM_YAML_IS_SEQUENCE_VECTOR(VPEYAML::Section)
LLVM_YAML_IS_SEQUENCE_VECTOR(VPEYAML::Symbol)
LLVM_YAML_IS_SEQUENCE_VECTOR(VPEYAML::Relocation)

namespace llvm {
namespace yaml {

template <>
struct ScalarEnumerationTraits<VPEYAML::WeakExternalCharacteristics> {
  static void enumeration(IO &IO, VPEYAML::WeakExternalCharacteristics &Value);
};

template <>
struct ScalarEnumerationTraits<VPEYAML::AuxSymbolType> {
  static void enumeration(IO &IO, VPEYAML::AuxSymbolType &Value);
};

template <>
struct ScalarEnumerationTraits<VPEYAML::COMDATType> {
  static void enumeration(IO &IO, VPEYAML::COMDATType &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::MachineTypes> {
  static void enumeration(IO &IO, VPE::MachineTypes &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::SymbolBaseType> {
  static void enumeration(IO &IO, VPE::SymbolBaseType &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::SymbolStorageClass> {
  static void enumeration(IO &IO, VPE::SymbolStorageClass &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::SymbolComplexType> {
  static void enumeration(IO &IO, VPE::SymbolComplexType &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::RelocationTypeI386> {
  static void enumeration(IO &IO, VPE::RelocationTypeI386 &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::RelocationTypeAMD64> {
  static void enumeration(IO &IO, VPE::RelocationTypeAMD64 &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::RelocationTypesARM> {
  static void enumeration(IO &IO, VPE::RelocationTypesARM &Value);
};

template <>
struct ScalarEnumerationTraits<VPE::RelocationTypesARM64> {
  static void enumeration(IO &IO, VPE::RelocationTypesARM64 &Value);
};

template <>
struct ScalarBitSetTraits<VPE::Characteristics> {
  static void bitset(IO &IO, VPE::Characteristics &Value);
};

template <>
struct ScalarBitSetTraits<VPE::SectionCharacteristics> {
  static void bitset(IO &IO, VPE::SectionCharacteristics &Value);
};

template <>
struct ScalarBitSetTraits<VPE::DLLCharacteristics> {
  static void bitset(IO &IO, VPE::DLLCharacteristics &Value);
};

template <>
struct MappingTraits<VPEYAML::Relocation> {
  static void mapping(IO &IO, VPEYAML::Relocation &Rel);
};

template <>
struct MappingTraits<VPEYAML::PEHeader> {
  static void mapping(IO &IO, VPEYAML::PEHeader &PH);
};

template <>
struct MappingTraits<VPE::DataDirectory> {
  static void mapping(IO &IO, VPE::DataDirectory &DD);
};

template <>
struct MappingTraits<VPE::header> {
  static void mapping(IO &IO, VPE::header &H);
};

template <> struct MappingTraits<VPE::AuxiliaryFunctionDefinition> {
  static void mapping(IO &IO, VPE::AuxiliaryFunctionDefinition &AFD);
};

template <> struct MappingTraits<VPE::AuxiliarybfAndefSymbol> {
  static void mapping(IO &IO, VPE::AuxiliarybfAndefSymbol &AAS);
};

template <> struct MappingTraits<VPE::AuxiliaryWeakExternal> {
  static void mapping(IO &IO, VPE::AuxiliaryWeakExternal &AWE);
};

template <> struct MappingTraits<VPE::AuxiliarySectionDefinition> {
  static void mapping(IO &IO, VPE::AuxiliarySectionDefinition &ASD);
};

template <> struct MappingTraits<VPE::AuxiliaryCLRToken> {
  static void mapping(IO &IO, VPE::AuxiliaryCLRToken &ACT);
};

template <>
struct MappingTraits<VPEYAML::Symbol> {
  static void mapping(IO &IO, VPEYAML::Symbol &S);
};

template <>
struct MappingTraits<VPEYAML::Section> {
  static void mapping(IO &IO, VPEYAML::Section &Sec);
};

template <>
struct MappingTraits<VPEYAML::Object> {
  static void mapping(IO &IO, VPEYAML::Object &Obj);
};

} // end namespace yaml
} // end namespace llvm

#endif // LLVM_OBJECTYAML_VPEYAML_H
