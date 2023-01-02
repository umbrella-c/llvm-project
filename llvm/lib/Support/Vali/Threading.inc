//===- Vali/Threading.inc - Vali Threading Implementation ----- -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the Vali specific implementation of Threading functions.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Errno.h"
#include "llvm/Support/ErrorHandling.h"

#include <os/mollenos.h>
#include <os/threads.h>
#include <threads.h>

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

// Include StrError(errnum) in a fatal error message.
[[noreturn]] static inline void ReportErrnumFatal(const char *Msg, int errnum) {
  std::string ErrMsg;
  MakeErrMsg(&ErrMsg, Msg, errnum);
  llvm::report_fatal_error(llvm::Twine(ErrMsg));
}

namespace {
  struct ThreadInfo {
    void*(*UserFn)(void *);
    void *UserData;
  };
}

static int ExecuteOnThread_Dispatch(void *Arg) {
  ThreadInfo *TI = reinterpret_cast<ThreadInfo*>(Arg);
  TI->UserFn(TI->UserData);
  return 0;
}

namespace llvm {
thrd_t llvm_execute_on_thread_impl(void *(*ThreadFunc)(void *), void *Arg,
                            Optional<unsigned> StackSizeInBytes) {
  ThreadInfo Info = { ThreadFunc, Arg };
  thrd_t Thread;
  int res;

  if (StackSizeInBytes) {
    // @todo
  }

  // Construct and execute the thread.
  res = ::thrd_create(&Thread, ExecuteOnThread_Dispatch, &Info);
  if (res != thrd_success)
    ReportErrnumFatal("thrd_create failed", res);
  return Thread;
}

void llvm_thread_detach_impl(thrd_t Thread) {
  int errnum;

  if ((errnum = ::thrd_detach(Thread)) != 0) {
    ReportErrnumFatal("thrd_detach failed", errnum);
  }
}

void llvm_thread_join_impl(thrd_t Thread) {
  int errnum;

  if ((errnum = ::thrd_join(Thread, nullptr)) != 0) {
    ReportErrnumFatal("thrd_join failed", errnum);
  }
}

thrd_t llvm_thread_get_id_impl(thrd_t Thread) {
  return Thread;
}

thrd_t llvm_thread_get_current_id_impl() {
  return ::thrd_current();
}
}

uint64_t llvm::get_threadid() {
  return uint64_t(thrd_current());
}


static constexpr uint32_t get_max_thread_name_length_impl() {
    return 64;
}

uint32_t llvm::get_max_thread_name_length() {
  return get_max_thread_name_length_impl();
}

void llvm::set_thread_name(const Twine &Name) {
  // Make sure the input is null terminated.
  SmallString<64> Storage;
  StringRef NameStr = Name.toNullTerminatedStringRef(Storage);

  // Truncate from the beginning, not the end, if the specified name is too
  // long.  For one, this ensures that the resulting string is still null
  // terminated, but additionally the end of a long thread name will usually
  // be more unique than the beginning, since a common pattern is for similar
  // threads to share a common prefix.
  if (get_max_thread_name_length() > 0)
    NameStr = NameStr.take_back(get_max_thread_name_length());
  (void)NameStr;
  ::ThreadsSetName(NameStr.data());
}

void llvm::get_thread_name(SmallVectorImpl<char> &Name) {
  Name.clear();
  char Buffer[get_max_thread_name_length_impl()] = { 0 };
  ThreadsGetName(&Buffer[0], get_max_thread_name_length_impl());
  Name.append(Buffer, Buffer + strlen(Buffer));
}

void llvm::ThreadPoolStrategy::apply_thread_strategy(
    unsigned ThreadPoolNum) const {}

#include <thread>
int computeHostNumHardwareThreads() {
  // Guard against std::thread::hardware_concurrency() returning 0.
  if (unsigned Val = std::thread::hardware_concurrency())
    return Val;
  return 1;
}

int llvm::get_physical_cores() {
  OSSystemCPUInfo_t cpuInfo;
  oserr_t           oserr;
  size_t            bytesQueried;

  oserr = OSSystemQuery(
          OSSYSTEMQUERY_CPUINFO,
          &cpuInfo,
          sizeof(OSSystemCPUInfo_t),
          &bytesQueried
  );
  if (oserr != OS_EOK) {
      return 1;
  }
  return (int)cpuInfo.NumberOfActiveCores;
}
