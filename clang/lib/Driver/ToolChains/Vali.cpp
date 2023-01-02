//===-- CrossWindows.cpp - Cross Windows Tool Chain -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Vali.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/SanitizerArgs.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;

using llvm::opt::ArgList;
using llvm::opt::ArgStringList;
using llvm::opt::Arg;

static bool IsCompilingNative(const ToolChain& TC)
{
  llvm::Triple HostTriple(LLVM_HOST_TRIPLE);
  return HostTriple.getOS() == TC.getTriple().getOS();
}

void tools::Vali::Assembler::ConstructJob(Compilation &C, const JobAction &JA,
                                          const InputInfo &Output,
                                          const InputInfoList &Inputs,
                                          const ArgList &Args,
                                          const char *LinkingOutput) const {
  claimNoWarnArgs(Args);
  const auto &TC =
      static_cast<const toolchains::ValiToolChain &>(getToolChain());
  ArgStringList CmdArgs;
  const char *Exec;

  switch (TC.getArch()) {
  default:
    llvm_unreachable("unsupported architecture");
  case llvm::Triple::arm:
  case llvm::Triple::thumb:
  case llvm::Triple::aarch64:
    break;
  case llvm::Triple::x86:
    CmdArgs.push_back("--32");
    break;
  case llvm::Triple::x86_64:
    CmdArgs.push_back("--64");
    break;
  }

  Args.AddAllArgValues(CmdArgs, options::OPT_Wa_COMMA, options::OPT_Xassembler);

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  for (const auto &Input : Inputs)
    CmdArgs.push_back(Input.getFilename());

  const std::string Assembler = TC.GetProgramPath("as");
  Exec = Args.MakeArgString(Assembler);

  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(), Exec, CmdArgs, Inputs));
}

void tools::Vali::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                       const InputInfo &Output,
                                       const InputInfoList &Inputs,
                                       const ArgList &Args,
                                       const char *LinkingOutput) const {
  const auto &TC =
      static_cast<const toolchains::ValiToolChain &>(getToolChain());
  const llvm::Triple &T = TC.getTriple();
  SmallString<128> EntryPoint;
  ArgStringList CmdArgs;
  const char *Exec;

  // Silence warning for "clang -g foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_g_Group);
  // and "clang -emit-llvm foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  // and for "clang -w foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_w);
  // Other warning options are already handled somewhere else.

  EntryPoint.append("-entry:");
  if (Args.hasArg(options::OPT_shared) || Args.hasArg(options::OPT_rdynamic)) {
    switch (T.getArch()) {
    default:
      llvm_unreachable("unsupported architecture");
    case llvm::Triple::aarch64:
    case llvm::Triple::arm:
    case llvm::Triple::thumb:
    case llvm::Triple::x86_64:
    case llvm::Triple::x86:
      EntryPoint.append("__CrtLibraryEntry");
      break;
    }

    CmdArgs.push_back(Args.MakeArgString("-dll"));

    SmallString<261> ImpLib(Output.getFilename());
    llvm::sys::path::replace_extension(ImpLib, ".dll.lib");
    CmdArgs.push_back(Args.MakeArgString(std::string("-implib:") + ImpLib));
  } else {
    EntryPoint.append("__CrtConsoleEntry");
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles)) {
    CmdArgs.push_back(Args.MakeArgString(EntryPoint));
  }
  CmdArgs.push_back("-lldvpe");

  CmdArgs.push_back(Args.MakeArgString(std::string("-out:") + Output.getFilename()));

  if (TC.getSanitizerArgs(Args).needsAsanRt()) {
    if (Args.hasArg(options::OPT_shared)) {
      CmdArgs.push_back(TC.getCompilerRTArgString(Args, "asan_dll_thunk"));
    } else {
      for (const auto &Lib : {"asan_dynamic", "asan_dynamic_runtime_thunk"})
        CmdArgs.push_back(TC.getCompilerRTArgString(Args, Lib));
      // Make sure the dynamic runtime thunk is not optimized out at link time
      // to ensure proper SEH handling.
      CmdArgs.push_back(Args.MakeArgString("--undefined"));
      CmdArgs.push_back(Args.MakeArgString(TC.getArch() == llvm::Triple::x86
                                               ? "___asan_seh_interceptor"
                                               : "__asan_seh_interceptor"));
    }
  }

  // handle inbuilt library paths
  auto addLibrarySearchPath = [&] (const Twine& path) {
    CmdArgs.push_back(Args.MakeArgString(Twine("-libpath:") + path));
  };

  if (IsCompilingNative(TC)) {
    const std::string LibaryPath = "$shr/lib";
    addLibrarySearchPath(LibaryPath);

    // If we are compiling native then try to find the linker through our
    // environmental variables
    Exec = Args.MakeArgString("$bin/lld-link");
  } else {
    // Cross-compiling to Vali, expect CROSS (backwards-compat) or VALICC (preferred) to be defined
    if (Optional<std::string> CrossCompilerPath =
            llvm::sys::Process::GetEnv("VALICC")) {
      Exec = Args.MakeArgString(*CrossCompilerPath + "/bin/lld-link");
    } else if (Optional<std::string> CrossCompilerPath =
            llvm::sys::Process::GetEnv("CROSS")) {
      Exec = Args.MakeArgString(*CrossCompilerPath + "/bin/lld-link");
    } else {
      // hope it's in path
      Exec = Args.MakeArgString("lld-link");
    }
    
    if (Optional<std::string> SdkPath =
            llvm::sys::Process::GetEnv("VALI_SDK_PATH")) {
      auto LibaryPath = *SdkPath + "/lib";
      addLibrarySearchPath(LibaryPath);
    }
  }
  
  // handle -L to -libpath conversion
  if (Args.hasArg(options::OPT_L)) {
    for (const auto &LibPath : Args.getAllArgValues(options::OPT_L)) {
      CmdArgs.push_back(Args.MakeArgString("-libpath:" + LibPath));
    }
  }
  
  // Add filenames, libraries, and other linker inputs.
  for (const auto &Input : Inputs) {
    if (Input.isFilename()) {
      CmdArgs.push_back(Input.getFilename());
      continue;
    }

    const Arg &A = Input.getInputArg();

    // Render -l options differently for the linker.
    if (A.getOption().matches(options::OPT_l)) {
      StringRef Lib = A.getValue();
      const char *LinkLibArg;
      if (Lib.endswith(".lib"))
        LinkLibArg = Args.MakeArgString(Lib);
      else
        LinkLibArg = Args.MakeArgString(Lib + ".lib");
      CmdArgs.push_back(LinkLibArg);
      continue;
    }

    // Otherwise, this is some other kind of linker input option like -Wl, -z,
    // or -L. Render it, even if MSVC doesn't understand it.
    A.renderAsInput(Args, CmdArgs);
  }

  if (TC.ShouldLinkCXXStdlib(Args)) {
    TC.AddCXXStdlibLibArgs(Args, CmdArgs);
  }

  if (!Args.hasArg(options::OPT_nostdlib)) {
    if (!Args.hasArg(options::OPT_nodefaultlibs)) {
      CmdArgs.push_back("c.dll.lib");
      CmdArgs.push_back("m.dll.lib");
      CmdArgs.push_back("librt.lib");
      CmdArgs.push_back("libcrt.lib");
      //AddRunTimeLibs(TC, D, CmdArgs, Args);
    }
  }

  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(), Exec, CmdArgs, Inputs));
}

ValiToolChain::ValiToolChain(const Driver &D, const llvm::Triple &T,
                             const llvm::opt::ArgList &Args)
    : ToolChain(D, T, Args) {}

ToolChain::UnwindTableLevel
ValiToolChain::getDefaultUnwindTableLevel(const ArgList &Args) const {
  // All non-x86_32 Windows targets require unwind tables. However, LLVM
  // doesn't know how to generate them for all targets, so only enable
  // the ones that are actually implemented.
  if (getArch() == llvm::Triple::x86_64 || getArch() == llvm::Triple::arm ||
      getArch() == llvm::Triple::thumb || getArch() == llvm::Triple::aarch64)
    return UnwindTableLevel::Asynchronous;

  return UnwindTableLevel::None;
}

bool ValiToolChain::isPICDefault() const {
  return getArch() == llvm::Triple::x86_64;
}

bool ValiToolChain::isPIEDefault(const llvm::opt::ArgList &Args) const {
  return false;
}

bool ValiToolChain::isPICDefaultForced() const {
  return getArch() == llvm::Triple::x86_64 ||
         getArch() == llvm::Triple::aarch64;
}

void ValiToolChain::AddClangSystemIncludeArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  const Driver &D = getDriver();

  auto AddSystemAfterIncludes = [&]() {
    for (const auto &P : DriverArgs.getAllArgValues(options::OPT_isystem_after))
      addSystemInclude(DriverArgs, CC1Args, P);
    
    if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
      SmallString<128> ResourceDir(D.ResourceDir);
      llvm::sys::path::append(ResourceDir, "include");
      addSystemInclude(DriverArgs, CC1Args, ResourceDir);
    }
  };

  if (DriverArgs.hasArg(options::OPT_nostdinc)) {
    AddSystemAfterIncludes();
    return;
  }

  if (IsCompilingNative(*this)) {
    const std::string SysRoot = "$shr/include";
    addSystemInclude(DriverArgs, CC1Args, SysRoot);
  } else {
    if (Optional<std::string> SdkIncludePath =
            llvm::sys::Process::GetEnv("VALI_SDK_PATH")) {
      auto IncludePath = *SdkIncludePath + "/include";
      addSystemInclude(DriverArgs, CC1Args, IncludePath);
    }
  }
  AddSystemAfterIncludes();
}

void ValiToolChain::AddClangCXXStdlibIncludeArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {

  if (DriverArgs.hasArg(options::OPT_nostdinc) ||
      DriverArgs.hasArg(options::OPT_nostdincxx))
    return;

  if (IsCompilingNative(*this)) {
    const std::string CxxStdInclude = "$shr/include/c++/v1";
    if (GetCXXStdlibType(DriverArgs) == ToolChain::CST_Libcxx)
      addSystemInclude(DriverArgs, CC1Args, CxxStdInclude);
  } else {
    if (GetCXXStdlibType(DriverArgs) == ToolChain::CST_Libcxx) {
      if (Optional<std::string> CrossCompilerPath = 
            llvm::sys::Process::GetEnv("VALI_SDK_PATH")) {
        auto IncludePath = *CrossCompilerPath + "/include/c++/v1";
        addSystemInclude(DriverArgs, CC1Args, IncludePath);
      }
    }
  }
}

void ValiToolChain::AddCXXStdlibLibArgs(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  bool StaticCXX = DriverArgs.hasArg(options::OPT_static_libstdcxx) && 
                  !DriverArgs.hasArg(options::OPT_static);
  if (GetCXXStdlibType(DriverArgs) == ToolChain::CST_Libcxx) {
    if (StaticCXX) {
      CC1Args.push_back("c++.lib");
      CC1Args.push_back("c++abi.lib");
    } else {
      CC1Args.push_back("c++.dll.lib");
    }
    CC1Args.push_back("unwind.dll.lib");
  }
}

clang::SanitizerMask ValiToolChain::getSupportedSanitizers() const {
  SanitizerMask Res = ToolChain::getSupportedSanitizers();
  Res |= SanitizerKind::Address;
  return Res;
}

Tool *ValiToolChain::buildLinker() const {
  return new tools::Vali::Linker(*this);
}

Tool *ValiToolChain::buildAssembler() const {
  return new tools::Vali::Assembler(*this);
}
