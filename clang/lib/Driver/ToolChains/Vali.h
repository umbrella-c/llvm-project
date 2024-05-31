//===--- CrossWindows.h - Vali ToolChain Implementation -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_VALI_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_VALI_H

#include "Cuda.h"
#include "Gnu.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {

namespace Vali {
class LLVM_LIBRARY_VISIBILITY Assembler : public Tool {
public:
  Assembler(const ToolChain &TC) : Tool("Vali::Assembler", "as", TC) {}

  bool hasIntegratedCPP() const override { return false; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  Linker(const ToolChain &TC)
      : Tool("Vali::Linker", "lld", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};
} // end namespace vali
} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY ValiToolChain : public ToolChain {
public:
  ValiToolChain(const Driver &D, const llvm::Triple &T,
                        const llvm::opt::ArgList &Args);

  bool IsIntegratedAssemblerDefault() const override { return true; }
  UnwindTableLevel
  getDefaultUnwindTableLevel(const llvm::opt::ArgList &Args) const override;
  bool isPICDefault() const override;
  bool isPIEDefault(const llvm::opt::ArgList &Args) const override;
  bool isPICDefaultForced() const override;
  
  RuntimeLibType GetDefaultRuntimeLibType() const override {
    return ToolChain::RLT_CompilerRT;
  }

  CXXStdlibType GetDefaultCXXStdlibType() const override {
    return ToolChain::CST_Libcxx;
  }

  UnwindLibType GetDefaultUnwindLibType() const override {
    return ToolChain::UNW_CompilerRT;
  }

  void AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                                 llvm::opt::ArgStringList &CC1Args) const override;
                        
  void AddClangCXXStdlibIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                                    llvm::opt::ArgStringList &CC1Args) const override;
  
  void AddCXXStdlibLibArgs(const llvm::opt::ArgList &Args,
                           llvm::opt::ArgStringList &CmdArgs) const override;

  SanitizerMask getSupportedSanitizers() const override;

protected:
  Tool *buildLinker() const override;
  Tool *buildAssembler() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_VALI_H
