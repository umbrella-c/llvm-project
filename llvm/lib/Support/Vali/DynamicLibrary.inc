//===- Vali/DynamicLibrary.cpp - Vali DL Implementation -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the Vali specific implementation of DynamicLibrary.
//
//===----------------------------------------------------------------------===//
#include <os/sharedobject.h>

DynamicLibrary::HandleSet::~HandleSet() {
  for (void *Handle : llvm::reverse(Handles))
    ::SharedObjectUnload(Handle_t(Handle));
  if (Process)
    ::SharedObjectUnload(Handle_t(Process));

  // llvm_shutdown called, Return to default
  DynamicLibrary::SearchOrder = DynamicLibrary::SO_Linker;
}

void *DynamicLibrary::HandleSet::DLOpen(const char *File, std::string *Err) {
    Handle_t Handle = SharedObjectLoad(File);
    if (Handle == HANDLE_INVALID) {
        if (Err) {
            if (File) {
                *Err = File;
            }
            else {
                *Err = "Failed to open handle on current library library(NULL)";
            }
        }
        return &DynamicLibrary::Invalid;
    }
    return reinterpret_cast<void*>(Handle);
}

void DynamicLibrary::HandleSet::DLClose(void *Handle) {
  ::SharedObjectUnload((Handle_t)Handle);
}

void *DynamicLibrary::HandleSet::DLSym(void *Handle, const char *Symbol) {
  return ::SharedObjectGetFunction((Handle_t)Handle, Symbol);
}

// Must declare the symbols in the global namespace.
static void *DoSearch(const char* SymbolName) {
#define EXPLICIT_SYMBOL(SYM) \
   extern void *SYM; if (!strcmp(SymbolName, #SYM)) return (void*)&SYM

  // If this is darwin, it has some funky issues, try to solve them here.  Some
  // important symbols are marked 'private external' which doesn't allow
  // SearchForAddressOfSymbol to find them.  As such, we special case them here,
  // there is only a small handful of them.

#ifdef __APPLE__
  {
    // __eprintf is sometimes used for assert() handling on x86.
    //
    // FIXME: Currently disabled when using Clang, as we don't always have our
    // runtime support libraries available.
#ifndef __clang__
#ifdef __i386__
    EXPLICIT_SYMBOL(__eprintf);
#endif
#endif
  }
#endif

#undef EXPLICIT_SYMBOL

// This macro returns the address of a well-known, explicit symbol
#define EXPLICIT_SYMBOL(SYM) if (!strcmp(SymbolName, #SYM)) return &SYM
  //EXPLICIT_SYMBOL(stderr);
  //EXPLICIT_SYMBOL(stdout);
  //EXPLICIT_SYMBOL(stdin);
#undef EXPLICIT_SYMBOL

  return nullptr;
}
