//===-- Vali.cpp - Vali Tool Chain -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Vali.h"
#include "clang/Basic/DiagnosticDriver.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Options/Options.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;

using llvm::opt::Arg;
using llvm::opt::ArgList;
using llvm::opt::ArgStringList;

static bool IsCompilingNative(const ToolChain &TC) {
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
    TC.getDriver().Diag(diag::err_drv_unsupported_opt_for_target)
        << "external assembly" << TC.getTriple().str();
    return;
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

  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileUTF8(),
                                         Exec, CmdArgs, Inputs, Output));
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
  if (!T.isOSBinFormatVPE()) {
    TC.getDriver().Diag(diag::err_drv_unsupported_opt_for_target)
        << "VPE linking" << T.str();
    return;
  }
  Args.claimAllArgs(options::OPT_rdynamic, options::OPT_static,
                    options::OPT_pthread);

  // Silence warning for "clang -g foo.o -o foo"
  Args.claimAllArgs(options::OPT_g_Group);
  // and "clang -emit-llvm foo.o -o foo"
  Args.claimAllArgs(options::OPT_emit_llvm);
  // and for "clang -w foo.o -o foo"
  Args.claimAllArgs(options::OPT_w);
  // Other warning options are already handled somewhere else.

  EntryPoint.append("-entry:");
  if (Args.hasArg(options::OPT_shared)) {
    EntryPoint.append("__CrtLibraryEntry");

    CmdArgs.push_back(Args.MakeArgString("-dll"));

    SmallString<261> ImpLib(Output.getFilename());
    llvm::sys::path::replace_extension(ImpLib, ".dll.lib");
    CmdArgs.push_back(Args.MakeArgString(std::string("-implib:") + ImpLib));
  } else {
    EntryPoint.append("__CrtConsoleEntry");
  }

  if (const Arg *A = Args.getLastArg(options::OPT_e))
    CmdArgs.push_back(Args.MakeArgString(Twine("-entry:") + A->getValue()));
  else if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles))
    CmdArgs.push_back(Args.MakeArgString(EntryPoint));
  CmdArgs.push_back("-lldvpe");
  if (const Arg *A = Args.getLastArg(options::OPT_g_Group))
    if (!A->getOption().matches(options::OPT_g0) &&
        !A->getOption().matches(options::OPT_ggdb0))
      CmdArgs.push_back("-debug:dwarf");

  CmdArgs.push_back(
      Args.MakeArgString(std::string("-out:") + Output.getFilename()));

  // handle inbuilt library paths
  auto addLibrarySearchPath = [&](const Twine &path) {
    CmdArgs.push_back(Args.MakeArgString(Twine("-libpath:") + path));
  };

  // Explicit linker selection overrides legacy toolchain environment paths.
  if (Args.hasArg(options::OPT_ld_path_EQ)) {
    Exec = Args.MakeArgString(TC.GetLinkerPath());
  } else {
    if (const Arg *A = Args.getLastArg(options::OPT_fuse_ld_EQ)) {
      if (StringRef(A->getValue()) != "lld") {
        TC.getDriver().Diag(diag::err_drv_invalid_linker_name)
            << A->getAsString(Args);
        return;
      }
    }
    if (IsCompilingNative(TC))
      Exec = Args.MakeArgString("$bin/lld-link");
    else if (auto Prefix = llvm::sys::Process::GetEnv("VALICC"))
      Exec = Args.MakeArgString(*Prefix + "/bin/lld-link");
    else if (auto Prefix = llvm::sys::Process::GetEnv("CROSS"))
      Exec = Args.MakeArgString(*Prefix + "/bin/lld-link");
    else
      Exec = Args.MakeArgString(TC.GetProgramPath("lld-link"));
  }
  for (const auto &Path : TC.getFilePaths())
    addLibrarySearchPath(Path);

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
      if (Lib.ends_with(".lib"))
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

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    for (const Arg *A : {Args.getLastArg(options::OPT_rtlib_EQ),
                         Args.getLastArg(options::OPT_unwindlib_EQ),
                         Args.getLastArg(options::OPT_stdlib_EQ)}) {
      if (!A)
        continue;
      StringRef Value = A->getValue();
      bool Supported = Value == "platform" ||
                       (A->getOption().matches(options::OPT_rtlib_EQ) &&
                        Value == "compiler-rt") ||
                       (A->getOption().matches(options::OPT_stdlib_EQ) &&
                        Value == "libc++") ||
                       (A->getOption().matches(options::OPT_unwindlib_EQ) &&
                        (Value == "libunwind" || Value == "none"));
      if (!Supported) {
        TC.getDriver().Diag(diag::err_drv_unsupported_opt_for_target)
            << A->getAsString(Args) << T.str();
        return;
      }
    }
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
    }
  }

  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileUTF8(),
                                         Exec, CmdArgs, Inputs, Output));
}

ValiToolChain::ValiToolChain(const Driver &D, const llvm::Triple &T,
                             const llvm::opt::ArgList &Args)
    : ToolChain(D, T, Args) {
  getProgramPaths().push_back(D.Dir);
  const std::string Root = computeSysRoot();
  if (!Root.empty())
    getFilePaths().push_back(Root + "/lib");
}

std::string ValiToolChain::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;
  if (auto Root = llvm::sys::Process::GetEnv("VALI_SDK_PATH"))
    return *Root;
  return IsCompilingNative(*this) ? "$shr" : "";
}

ToolChain::UnwindTableLevel
ValiToolChain::getDefaultUnwindTableLevel(const ArgList &Args) const {
  // Preserve the original Vali unwind-table defaults. VPE uses DWARF.
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

void ValiToolChain::AddClangSystemIncludeArgs(const ArgList &Args,
                                              ArgStringList &CC1Args) const {
  if (Args.hasArg(options::OPT_nostdinc))
    return;
  if (!Args.hasArg(options::OPT_nostdlibinc)) {
    const std::string Root = computeSysRoot();
    if (!Root.empty())
      addSystemInclude(Args, CC1Args, Root + "/include");
  }
  for (const auto &Path : Args.getAllArgValues(options::OPT_isystem_after))
    addSystemInclude(Args, CC1Args, Path);
  if (!Args.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> ResourceDir(getDriver().ResourceDir);
    llvm::sys::path::append(ResourceDir, "include");
    addSystemInclude(Args, CC1Args, ResourceDir);
  }
}

void ValiToolChain::AddClangCXXStdlibIncludeArgs(const ArgList &Args,
                                                 ArgStringList &CC1Args) const {
  if (Args.hasArg(options::OPT_nostdinc, options::OPT_nostdincxx,
                  options::OPT_nostdlibinc))
    return;
  const std::string Root = computeSysRoot();
  if (!Root.empty() && GetCXXStdlibType(Args) == ToolChain::CST_Libcxx)
    addSystemInclude(Args, CC1Args, Root + "/include/c++/v1");
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
    if (DriverArgs.getLastArgValue(options::OPT_unwindlib_EQ) != "none")
      CC1Args.push_back("unwind.dll.lib");
  }
}

Tool *ValiToolChain::buildLinker() const {
  return new tools::Vali::Linker(*this);
}

Tool *ValiToolChain::buildAssembler() const {
  return new tools::Vali::Assembler(*this);
}
