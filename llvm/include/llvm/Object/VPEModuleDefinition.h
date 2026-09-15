//===--- VPEModuleDefinition.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Windows-specific.
// A parser for the module-definition file (.def file).
// Parsed results are directly written to Config global variable.
//
// The format of module-definition files are described in this document:
// https://msdn.microsoft.com/en-us/library/28d6s79h.aspx
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_OBJECT_VPE_MODULE_DEFINITION_H
#define LLVM_OBJECT_VPE_MODULE_DEFINITION_H

#include "llvm/Object/VPE.h"
#include "llvm/Object/VPEImportFile.h"

namespace llvm {
namespace object {

struct VPEModuleDefinition {
  std::vector<VPEShortExport> Exports;
  std::string OutputFile;
  std::string ImportName;
  uint64_t ImageBase = 0;
  uint64_t StackReserve = 0;
  uint64_t StackCommit = 0;
  uint64_t HeapReserve = 0;
  uint64_t HeapCommit = 0;
  uint32_t MajorImageVersion = 0;
  uint32_t MinorImageVersion = 0;
  uint32_t MajorOSVersion = 0;
  uint32_t MinorOSVersion = 0;
};

Expected<VPEModuleDefinition>
parseVPEModuleDefinition(MemoryBufferRef MB, VPE::MachineTypes Machine);

} // End namespace object.
} // End namespace llvm.

#endif
