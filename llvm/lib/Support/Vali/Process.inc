//===- Vali/Process.cpp - Vali Process Implementation ------- -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the Vali specific implementation of the Process class.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/Allocator.h"
#include "llvm/Support/ErrorHandling.h"
#include <os/mollenos.h>
#include <os/process.h>
#include <malloc.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <chrono>

using namespace llvm;

Process::Pid Process::getProcessId() {
  static_assert(sizeof(Pid) >= sizeof(uint32_t),
                "Process::Pid should be big enough to store DWORD");
  return Pid(::ProcessGetCurrentId());
}

// This function retrieves the page size using GetNativeSystemInfo() and is
// present solely so it can be called once to initialize the self_process member
// below.
static unsigned computePageSize() {
  SystemDescriptor_t Descriptor;
    if (SystemQuery(&Descriptor) != OsSuccess) {
        // Default to 0x1000
        return 0x1000;
    }
    return Descriptor.PageSizeBytes;
}

Expected<unsigned> Process::getPageSize() {
  static unsigned Ret = computePageSize();
  return Ret;
}

size_t Process::GetMallocUsage() {
  struct mallinfo mi;
  mi = ::mallinfo();
  return mi.uordblks;
}

void Process::GetTimeUsage(TimePoint<> &elapsed, std::chrono::nanoseconds &user_time,
                           std::chrono::nanoseconds &sys_time) {
  struct timespec ptime, stime;
  timespec_get(&ptime, TIME_PROCESS);
  timespec_get(&stime, TIME_MONOTONIC);
  elapsed = std::chrono::system_clock::now();
  user_time = std::chrono::nanoseconds(ptime.tv_sec * NSEC_PER_SEC + ptime.tv_nsec);
  sys_time = std::chrono::nanoseconds(stime.tv_sec * NSEC_PER_SEC + stime.tv_nsec);
}

// Some LLVM programs such as bugpoint produce core files as a normal part of
// their operation. To prevent the disk from filling up, this configuration
// item does what's necessary to prevent their generation.
void Process::PreventCoreFiles() {
  coreFilesPrevented = true;
}

/// Returns the environment variable \arg Name's value as a string encoded in
/// UTF-8. \arg Name is assumed to be in UTF-8 encoding.
Optional<std::string> Process::GetEnv(StringRef Name) {
  std::string NameStr = Name.str();
  const char *Val = ::getenv(NameStr.c_str());
  if (!Val)
    return None;
  return std::string(Val);
}

std::error_code Process::FixupStandardFileDescriptors() {
  return std::error_code();
}

std::error_code Process::SafelyCloseFileDescriptor(int FD) {
  if (::close(FD) < 0)
    return std::error_code(errno, std::generic_category());
  return std::error_code();
}

bool Process::StandardInIsUserInput() {
  return FileDescriptorIsDisplayed(STDIN_FILENO);
}

bool Process::StandardOutIsDisplayed() {
  return FileDescriptorIsDisplayed(STDOUT_FILENO);
}

bool Process::StandardErrIsDisplayed() {
  return FileDescriptorIsDisplayed(STDERR_FILENO);
}

bool Process::FileDescriptorIsDisplayed(int fd) {
  return isatty(fd);
}

static unsigned getColumns(int FileID) {
  // If COLUMNS is defined in the environment, wrap to that many columns.
  if (const char *ColumnsStr = std::getenv("COLUMNS")) {
    int Columns = std::atoi(ColumnsStr);
    if (Columns > 0)
      return Columns;
  }
  unsigned Columns = 0;
  return Columns;
}

unsigned Process::StandardOutColumns() {
  if (!StandardOutIsDisplayed())
    return 0;

  return getColumns(1);
}

unsigned Process::StandardErrColumns() {
  if (!StandardErrIsDisplayed())
    return 0;

  return getColumns(2);
}

static bool terminalHasColors(int fd) {
  return false;
}

bool Process::FileDescriptorHasColors(int fd) {
  // A file descriptor has colors if it is displayed and the terminal has
  // colors.
  return FileDescriptorIsDisplayed(fd) && terminalHasColors(fd);
}

bool Process::StandardOutHasColors() {
  return FileDescriptorHasColors(STDOUT_FILENO);
}

bool Process::StandardErrHasColors() {
  return FileDescriptorHasColors(STDERR_FILENO);
}

void Process::UseANSIEscapeCodes(bool /*enable*/) {
  // No effect.
}

bool Process::ColorNeedsFlush() {
  // No, we use ANSI escape sequences.
  return false;
}

const char *Process::OutputColor(char code, bool bold, bool bg) {
  return colorcodes[bg?1:0][bold?1:0][code&7];
}

const char *Process::OutputBold(bool bg) {
  return "\033[1m";
}

const char *Process::OutputReverse() {
  return "\033[7m";
}

const char *Process::ResetColor() {
  return "\033[0m";
}

unsigned Process::GetRandomNumber() {
  const auto Now = std::chrono::high_resolution_clock::now();
  auto Seed = hash_combine(Now.time_since_epoch().count(), ::ProcessGetCurrentId());
  static int x = (static_cast<void>(::srand(Seed)), 0);
  (void)x;
  return ::rand();
}

LLVM_ATTRIBUTE_NORETURN
void Process::ExitNoCleanup(int RetCode) {
  _Exit(RetCode);
  llvm_unreachable("_Exit doesn't return");
}
