//===- ValiTargetTest.cpp - Vali target ABI tests
//---------------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/raw_ostream.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

using namespace clang;

TEST(ValiTargetTest, ArchitectureABI) {
  for (const char *Arch :
       {"i386", "x86_64", "armv7", "thumbv7", "armebv7", "thumbebv7", "aarch64",
        "aarch64_be", "mips", "mipsel", "mips64", "mips64el"}) {
    DiagnosticOptions DiagOpts;
    DiagnosticsEngine Diags(DiagnosticIDs::create(), DiagOpts,
                            new IgnoringDiagConsumer());
    TargetOptions Opts;
    Opts.Triple = std::string(Arch) + "-uml-vali";
    SCOPED_TRACE(Opts.Triple);
    std::unique_ptr<TargetInfo> T(TargetInfo::CreateTargetInfo(Diags, Opts));
    ASSERT_NE(nullptr, T);
    EXPECT_FALSE(Diags.hasErrorOccurred());
    EXPECT_EQ(TargetInfo::UnsignedShort, T->getWCharType());
    EXPECT_EQ(TargetInfo::UnsignedShort, T->getWIntType());
    std::string Defines;
    llvm::raw_string_ostream OS(Defines);
    MacroBuilder Builder(OS);
    LangOptions LangOpts;
    T->getTargetDefines(LangOpts, Builder);
    for (const char *Name : {"VALI", "MOLLENOS", "__VALI__", "__MOLLENOS__"})
      EXPECT_THAT(Defines,
                  testing::HasSubstr(std::string("#define ") + Name + " 1\n"));
    EXPECT_THAT(Defines, testing::Not(testing::HasSubstr("#define _WIN32 ")));
    if (T->getTriple().isArch64Bit()) {
      EXPECT_THAT(Defines, testing::HasSubstr("#define __VALI64__ 1\n"));
      EXPECT_THAT(Defines,
                  testing::HasSubstr("#define _INTEGRAL_MAX_BITS 64\n"));
    } else {
      EXPECT_THAT(Defines,
                  testing::HasSubstr("#define _INTEGRAL_MAX_BITS 32\n"));
    }
    if (T->getTriple().isX86()) {
      EXPECT_TRUE(T->shouldUseMicrosoftCCforMangling());
      EXPECT_STREQ(T->getTriple().isArch32Bit() ? "_" : "",
                   T->getUserLabelPrefix());
      EXPECT_EQ(32u, T->getLongWidth());
      EXPECT_EQ(64u, T->getDoubleAlign());
      EXPECT_EQ(64u, T->getLongLongAlign());
    }
    if (T->getTriple().getArch() == llvm::Triple::x86_64) {
      EXPECT_EQ(64u, T->getLongDoubleWidth());
      EXPECT_EQ(&llvm::APFloat::IEEEdouble(), &T->getLongDoubleFormat());
      EXPECT_EQ(TargetInfo::UnsignedLongLong, T->getSizeType());
      EXPECT_EQ(TargetInfo::SignedLongLong, T->getPtrDiffType(LangAS::Default));
      EXPECT_EQ(TargetInfo::CharPtrBuiltinVaList, T->getBuiltinVaListKind());
      EXPECT_EQ(TargetInfo::CCK_MicrosoftWin64, T->getCallingConvKind(false));
    }
  }
}

TEST(ValiTargetTest, OtherX86Platforms) {
  for (const char *Triple :
       {"i386-pc-windows-msvc", "x86_64-pc-windows-msvc",
        "i386-unknown-linux-gnu", "x86_64-unknown-linux-gnu"}) {
    DiagnosticOptions DiagOpts;
    DiagnosticsEngine Diags(DiagnosticIDs::create(), DiagOpts,
                            new IgnoringDiagConsumer());
    TargetOptions Opts;
    Opts.Triple = Triple;
    SCOPED_TRACE(Triple);
    std::unique_ptr<TargetInfo> T(TargetInfo::CreateTargetInfo(Diags, Opts));
    ASSERT_NE(nullptr, T);
    EXPECT_FALSE(Diags.hasErrorOccurred());
    EXPECT_FALSE(T->getTriple().isOSVali());
    const bool Windows = T->getTriple().isOSWindows();
    EXPECT_EQ(Windows || T->getTriple().isArch32Bit() ? 32u : 64u,
              T->getLongWidth());
    EXPECT_EQ(Windows ? TargetInfo::UnsignedShort : TargetInfo::SignedInt,
              T->getWCharType());
    EXPECT_EQ(Windows, T->shouldUseMicrosoftCCforMangling());
    std::string Defines;
    llvm::raw_string_ostream OS(Defines);
    MacroBuilder Builder(OS);
    T->getTargetDefines(LangOptions(), Builder);
    EXPECT_THAT(Defines, testing::Not(testing::HasSubstr("#define VALI ")));
    EXPECT_THAT(Defines, testing::Not(testing::HasSubstr("#define MOLLENOS ")));
  }
}
