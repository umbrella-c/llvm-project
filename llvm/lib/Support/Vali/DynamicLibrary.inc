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
#include <os/services/sharedobject.h>

DynamicLibrary::HandleSet::~HandleSet() {
  for (void *Handle : llvm::reverse(Handles))
    ::OSLibraryUnload(Handle_t(Handle));
  if (Process)
    ::OSLibraryUnload(Handle_t(Process));

  // llvm_shutdown called, Return to default
  DynamicLibrary::SearchOrder = DynamicLibrary::SO_Linker;
}

void *DynamicLibrary::HandleSet::DLOpen(const char *File, std::string *Err) {
    Handle_t Handle = OSLibraryLoad(File);
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
  ::OSLibraryUnload((Handle_t)Handle);
}

void *DynamicLibrary::HandleSet::DLSym(void *Handle, const char *Symbol) {
  return ::OSLibraryLookupFunction((Handle_t)Handle, Symbol);
}

// Must declare the symbols in the global namespace.
static void *DoSearch(const char* SymbolName) {
// This macro returns the address of a well-known, explicit symbol
#define EXPLICIT_SYMBOL(SYM) if (!strcmp(SymbolName, #SYM)) return &SYM
  //EXPLICIT_SYMBOL(stderr);
  //EXPLICIT_SYMBOL(stdout);
  //EXPLICIT_SYMBOL(stdin);
#undef EXPLICIT_SYMBOL
  return nullptr;
}
