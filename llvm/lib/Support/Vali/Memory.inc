//===- Vali/Memory.cpp - Vali Memory Implementation -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the Vali specific implementation of various Memory
// management utilities
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Process.h"
#include <os/memory.h>
#include <os/mollenos.h>

namespace {

unsigned int getValiProtectionFlags(unsigned int Flags) {
    unsigned int Result = 0;
    if (Flags & llvm::sys::Memory::MF_READ) {
        Result |= MEMORY_READ;
    }
    if (Flags & llvm::sys::Memory::MF_WRITE) {
        Result |= MEMORY_WRITE;
    }
    if (Flags & llvm::sys::Memory::MF_EXEC) {
        Result |= MEMORY_EXECUTABLE;
    }
    return Result;
}

size_t getPageSize() {
    OSSystemMemoryInfo_t memInfo;
    oserr_t              oserr;
    size_t               bytesQueried;

    oserr = OSSystemQuery(
            OSSYSTEMQUERY_MEMINFO,
            &memInfo,
            sizeof(OSSystemMemoryInfo_t),
            &bytesQueried
    );
    if (oserr != OS_EOK) {
        // Default to 0x1000
        return 0x1000;
    }
    return memInfo.PageSizeBytes;
}

} // namespace

namespace llvm {
namespace sys {

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only Vali specific code
//===          and must not be UNIX code
//===----------------------------------------------------------------------===//

MemoryBlock Memory::allocateMappedMemory(size_t NumBytes,
                                         const MemoryBlock *const NearBlock,
                                         unsigned Flags,
                                         std::error_code &EC) {
  EC = std::error_code();
  if (NumBytes == 0)
    return MemoryBlock();

  static size_t DefaultGranularity = getPageSize();

  size_t Granularity     = DefaultGranularity;
  unsigned int Protect   = MEMORY_COMMIT;
  const size_t NumBlocks = (NumBytes + Granularity - 1) / Granularity;

  uintptr_t Start = NearBlock ? reinterpret_cast<uintptr_t>(NearBlock->base()) +
                                NearBlock->allocatedSize()
                           : 0;

  // If the requested address is not aligned to the allocation granularity,
  // round up to get beyond NearBlock. VirtualAlloc would have rounded down.
  if (Start && Start % Granularity != 0)
    Start += Granularity - Start % Granularity;

  Protect |= getValiProtectionFlags(Flags);
  void *PA = NULL;
  if (::MemoryAllocate(reinterpret_cast<void*>(Start),
                       NumBlocks*Granularity, Protect, &PA) != OS_EOK) {
    return MemoryBlock();
  }

  MemoryBlock Result;
  Result.Address       = PA;
  Result.AllocatedSize = NumBlocks*Granularity;
  Result.Flags         = Flags;

  if (Flags & MF_EXEC)
    Memory::InvalidateInstructionCache(Result.Address, Result.AllocatedSize);

  return Result;
}

  std::error_code Memory::releaseMappedMemory(MemoryBlock &M) {
  if (M.Address == 0 || M.AllocatedSize == 0)
    return std::error_code();

  if (MemoryFree(M.Address, M.AllocatedSize) != OS_EOK)
    return std::error_code();

  M.Address = 0;
  M.AllocatedSize = 0;

  return std::error_code();
}

  std::error_code Memory::protectMappedMemory(const MemoryBlock &M,
                                       unsigned Flags) {
  if (M.Address == 0 || M.AllocatedSize == 0)
    return std::error_code();

  unsigned int Protect = getValiProtectionFlags(Flags);
  unsigned int Previous = 0;
  if (MemoryProtect(M.Address, M.AllocatedSize, Protect, &Previous) != OS_EOK)
    return std::error_code();

  if (Flags & MF_EXEC)
    Memory::InvalidateInstructionCache(M.Address, M.AllocatedSize);

  return std::error_code();
}

/// InvalidateInstructionCache - Before the JIT can run a block of code
/// that has been emitted it must invalidate the instruction cache on some
/// platforms.
void Memory::InvalidateInstructionCache(const void *Addr, size_t Len) {
  FlushHardwareCache(CACHE_INSTRUCTION, const_cast<void*>(Addr), Len);
}

} // namespace sys
} // namespace llvm
