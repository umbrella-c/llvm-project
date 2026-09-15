//===- Vali/Program.cpp - Vali Program Implementation ------- -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the Vali specific implementation of the Program class.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <io.h>
#include <malloc.h>
#include <os/mollenos.h>
#include <os/services/process.h>
#include <signal.h>

namespace llvm {

ProcessInfo::ProcessInfo() : Pid(UUID_INVALID) {}

ErrorOr<std::string> sys::findProgramByName(StringRef Name,
                                            ArrayRef<StringRef> Paths) {
  assert(!Name.empty() && "Must have a name!");
  // Use the given path verbatim if it contains any slashes; this matches
  // the behavior of sh(1) and friends.
  if (Name.find_first_of("/\\") != StringRef::npos)
    return std::string(Name);

  SmallVector<StringRef, 12> PathExts;
  PathExts.push_back("");
  PathExts.push_back(".run");

  SmallVector<StringRef, 16> EnvironmentPaths;
  if (Paths.empty())
    if (const char *PathEnv = std::getenv("PATH")) {
      SplitString(PathEnv, EnvironmentPaths, ";");
      Paths = EnvironmentPaths;
    }
  
  for (StringRef Ext : PathExts) {
    for (auto Path : Paths) {
        if (Path.empty())
            continue;

        // Check to see if this first directory contains the executable...
        SmallString<128> FilePath(Path);
        sys::path::append(FilePath, Name);
        sys::path::append(FilePath, Ext);
        if (sys::fs::can_execute(FilePath.c_str()))
            return std::string(FilePath.str()); // Found the executable!
    }
  }
  
  return errc::no_such_file_or_directory;
}

/// ArgNeedsQuotes - Check whether argument needs to be quoted when calling
/// CreateProcess.
static bool argNeedsQuotes(StringRef Arg) {
  if (Arg.empty())
    return true;
  return StringRef::npos != Arg.find_first_of("\t \"&\'()*<>\\`^|");
}

static std::string quoteSingleArg(StringRef Arg) {
  std::string Result;
  Result.push_back('"');

  while (!Arg.empty()) {
    size_t FirstNonBackslash = Arg.find_first_not_of('\\');
    size_t BackslashCount = FirstNonBackslash;
    if (FirstNonBackslash == StringRef::npos) {
      // The entire remainder of the argument is backslashes.  Escape all of
      // them and just early out.
      BackslashCount = Arg.size();
      Result.append(BackslashCount * 2, '\\');
      break;
    }

    if (Arg[FirstNonBackslash] == '\"') {
      // This is an embedded quote.  Escape all preceding backslashes, then
      // add one additional backslash to escape the quote.
      Result.append(BackslashCount * 2 + 1, '\\');
      Result.push_back('\"');
    } else {
      // This is just a normal character.  Don't escape any of the preceding
      // backslashes, just append them as they are and then append the
      // character.
      Result.append(BackslashCount, '\\');
      Result.push_back(Arg[FirstNonBackslash]);
    }

    // Drop all the backslashes, plus the following character.
    Arg = Arg.drop_front(FirstNonBackslash + 1);
  }

  Result.push_back('"');
  return Result;
}

ErrorOr<std::string> sys::flattenArgs(ArrayRef<StringRef> Args) {
  std::string Command;
  for (StringRef Arg : Args) {
    if (argNeedsQuotes(Arg))
      Command += quoteSingleArg(Arg);
    else
      Command += Arg;

    Command.push_back(' ');
  }

  return Command;
}
}

// Implemented
static bool Execute(ProcessInfo &PI, StringRef Program,
                    ArrayRef<StringRef> Args, Optional<ArrayRef<StringRef>> Env,
                    ArrayRef<Optional<StringRef>> Redirects,
                    unsigned MemoryLimit, std::string *ErrMsg,
                    BitVector *AffinityMask) {
  if (!sys::fs::can_execute(Program)) {
    if (ErrMsg)
      *ErrMsg = "program not executable";
    return false;
  }

  // can_execute may succeed by looking at Program + ".run". 
  SmallString<64> ProgramStorage;
  if (!sys::fs::exists(Program))
    Program = Twine(Program + ".run").toStringRef(ProgramStorage);

  // Vali wants a command line, not an array of args, to pass to the new
  // process.  We have to concatenate them all, while quoting the args that
  // have embedded spaces (or are empty).
  ErrorOr<std::string> Command = flattenArgs(Args);
  oserr_t status = OSProcessSpawn(Program.begin(), Command->c_str(), &PI.Pid);
  if (status != OS_EOK) {
      return false;
  }
  return true;
}

/// This function builds an error message into \p ErrMsg using the \p prefix
/// string and the Unix error number given by \p errnum. If errnum is -1, the
/// default then the value of errno is used.
/// Make an error message
///
/// If the error number can be converted to a string, it will be
/// separated from prefix by ": ".
static inline bool MakeErrMsg(
  std::string* ErrMsg, const std::string& prefix, int errnum = -1) {
  if (!ErrMsg)
    return true;
  if (errnum == -1)
    errnum = errno;
  *ErrMsg = prefix + ": " + llvm::sys::StrError(errnum);
  return true;
}

// Implemented
namespace llvm {
ProcessInfo sys::Wait(const ProcessInfo &PI,
                      std::optional<unsigned> SecondsToWait,
                      std::string *ErrMsg,
                      std::optional<ProcessStatistics> *ProcStat,
                      bool Polling) {
  assert(PI.Pid && "invalid pid to wait on, process not started?");
  assert(PI.Pid != UUID_INVALID &&
         "invalid process handle to wait on, process not started?");
  size_t      milliSecondsToWait = SecondsToWait ? *SecondsToWait * 1000 : 0;
  oserr_t     oserr;
  ProcessInfo WaitResult = PI;
  
  if (ProcStat)
    ProcStat->reset();

  oserr = OSProcessJoin(PI.Pid, milliSecondsToWait, &WaitResult.ReturnCode);
  if (oserr == OS_ETIMEOUT) {
    if (!Polling && *SecondsToWait > 0) {
      oserr = OSProcessSignal(PI.Pid, SIGKILL);
      if (oserr != OS_EOK) {
        if (ErrMsg)
          MakeErrMsg(ErrMsg, "Failed to terminate timed-out program");

        // -2 indicates a crash or timeout as opposed to failure to execute.
        WaitResult.ReturnCode = -2;
        return WaitResult;
      }
      (void)OSProcessJoin(PI.Pid, 0, &WaitResult.ReturnCode);
    } else {
      // Non-blocking wait.
      return ProcessInfo();
    }
  }

  // Get process execution statistics.
  if (ProcStat) {
    /** 
     * TODO: Implement these statistics
    FILETIME CreationTime, ExitTime, KernelTime, UserTime;
    PROCESS_MEMORY_COUNTERS MemInfo;
    if (GetProcessTimes(PI.Process, &CreationTime, &ExitTime, &KernelTime,
                        &UserTime) &&
        GetProcessMemoryInfo(PI.Process, &MemInfo, sizeof(MemInfo))) {
      auto UserT = std::chrono::duration_cast<std::chrono::microseconds>(
          toDuration(UserTime));
      auto KernelT = std::chrono::duration_cast<std::chrono::microseconds>(
          toDuration(KernelTime));
      uint64_t PeakMemory = MemInfo.PeakPagefileUsage / 1024;
      *ProcStat = ProcessStatistics{UserT + KernelT, UserT, PeakMemory};
    }
    */
  }
  return WaitResult;
}

std::error_code llvm::sys::ChangeStdinMode(fs::OpenFlags Flags){
  if (!(Flags & fs::OF_Text))
    return ChangeStdinToBinary();
  return std::error_code();
}

std::error_code llvm::sys::ChangeStdoutMode(fs::OpenFlags Flags){
  if (!(Flags & fs::OF_Text))
    return ChangeStdoutToBinary();
  return std::error_code();
}

// Implemented
  std::error_code sys::ChangeStdinToBinary(){
  // Do nothing, as Vali doesn't differentiate between text and binary.
    return std::error_code();
}

// Implemented
  std::error_code sys::ChangeStdoutToBinary(){
  // Do nothing, as Vali doesn't differentiate between text and binary.
    return std::error_code();
}

// Implemented
std::error_code
llvm::sys::writeFileWithEncoding(StringRef FileName, StringRef Contents,
                                 WindowsEncodingMethod Encoding /*unused*/) {
  std::error_code EC;
  llvm::raw_fd_ostream OS(FileName, EC, llvm::sys::fs::OpenFlags::OF_Text);

  if (EC)
    return EC;

  OS << Contents;

  if (OS.has_error())
    return make_error_code(errc::io_error);

  return EC;
}

bool llvm::sys::commandLineFitsWithinSystemLimits(StringRef Program,
                                                  ArrayRef<StringRef> Args) {
  // The documented max length of the command line passed to CreateProcess.
  static const size_t MaxCommandStringLength = 32768;
  SmallVector<StringRef, 8> FullArgs;
  FullArgs.push_back(Program);
  FullArgs.append(Args.begin(), Args.end());
  ErrorOr<std::string> Result = flattenArgs(FullArgs);
  return (Result->size() + 1) <= MaxCommandStringLength;
}

}
