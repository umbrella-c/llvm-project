//===- Signals.cpp - Generic Unix Signals Implementation -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines some helpful functions for dealing with the possibility of
// Unix signals occurring while your program is running.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/Config/config.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/raw_ostream.h"
#include <os/mollenos.h>
#include <algorithm>
#include <unwind.h>
#include <signal.h>
#include <assert.h>
#include <string>
#include <io.h>

using namespace llvm;

static RETSIGTYPE SignalHandler(int Sig);  // defined below.

using SignalHandlerFunctionType = void (*)();
/// The function to call if ctrl-c is pressed.
static std::atomic<SignalHandlerFunctionType> InterruptFunction =
    ATOMIC_VAR_INIT(nullptr);
static std::atomic<SignalHandlerFunctionType> InfoSignalFunction =
    ATOMIC_VAR_INIT(nullptr);
/// The function to call on SIGPIPE (one-time use only).
static std::atomic<SignalHandlerFunctionType> OneShotPipeSignalFunction =
    ATOMIC_VAR_INIT(nullptr);

namespace {
/// Signal-safe removal of files.
/// Inserting and erasing from the list isn't signal-safe, but removal of files
/// themselves is signal-safe. Memory is freed when the head is freed, deletion
/// is therefore not signal-safe either.
class FileToRemoveList {
  std::atomic<char *> Filename = ATOMIC_VAR_INIT(nullptr);
  std::atomic<FileToRemoveList *> Next = ATOMIC_VAR_INIT(nullptr);

  FileToRemoveList() = default;
  // Not signal-safe.
  FileToRemoveList(const std::string &str) : Filename(strdup(str.c_str())) {}

public:
  // Not signal-safe.
  ~FileToRemoveList() {
    if (FileToRemoveList *N = Next.exchange(nullptr))
      delete N;
    if (char *F = Filename.exchange(nullptr))
      free(F);
  }

  // Not signal-safe.
  static void insert(std::atomic<FileToRemoveList *> &Head,
                     const std::string &Filename) {
    // Insert the new file at the end of the list.
    FileToRemoveList *NewHead = new FileToRemoveList(Filename);
    std::atomic<FileToRemoveList *> *InsertionPoint = &Head;
    FileToRemoveList *OldHead = nullptr;
    while (!InsertionPoint->compare_exchange_strong(OldHead, NewHead)) {
      InsertionPoint = &OldHead->Next;
      OldHead = nullptr;
    }
  }

  // Not signal-safe.
  static void erase(std::atomic<FileToRemoveList *> &Head,
                    const std::string &Filename) {
    // Use a lock to avoid concurrent erase: the comparison would access
    // free'd memory.
    static ManagedStatic<sys::SmartMutex<true>> Lock;
    sys::SmartScopedLock<true> Writer(*Lock);

    for (FileToRemoveList *Current = Head.load(); Current;
         Current = Current->Next.load()) {
      if (char *OldFilename = Current->Filename.load()) {
        if (OldFilename != Filename)
          continue;
        // Leave an empty filename.
        OldFilename = Current->Filename.exchange(nullptr);
        // The filename might have become null between the time we
        // compared it and we exchanged it.
        if (OldFilename)
          free(OldFilename);
      }
    }
  }

  // Signal-safe.
  static void removeAllFiles(std::atomic<FileToRemoveList *> &Head) {
    // If cleanup were to occur while we're removing files we'd have a bad time.
    // Make sure we're OK by preventing cleanup from doing anything while we're
    // removing files. If cleanup races with us and we win we'll have a leak,
    // but we won't crash.
    FileToRemoveList *OldHead = Head.exchange(nullptr);

    for (FileToRemoveList *currentFile = OldHead; currentFile;
         currentFile = currentFile->Next.load()) {
      // If erasing was occuring while we're trying to remove files we'd look
      // at free'd data. Take away the path and put it back when done.
      if (char *path = currentFile->Filename.exchange(nullptr)) {
        // Get the status so we can determine if it's a file or directory. If we
        // can't stat the file, ignore it.
        OsFileDescriptor_t buf;
        if (GetFileInformationFromPath(path, &buf) != OsSuccess)
          continue;

        // If this is not a regular file, ignore it. We want to prevent removal
        // of special files like /dev/null, even if the compiler is being run
        // with the super-user permissions.
        if (!(buf.Flags & FILE_FLAG_FILE))
          continue;

        // Otherwise, remove the file. We ignore any errors here as there is
        // nothing else we can do.
        unlink(path);

        // We're done removing the file, erasing can safely proceed.
        currentFile->Filename.exchange(path);
      }
    }

    // We're done removing files, cleanup can safely proceed.
    Head.exchange(OldHead);
  }
};

static std::atomic<FileToRemoveList *> FilesToRemove = ATOMIC_VAR_INIT(nullptr);

/// Clean up the list in a signal-friendly manner.
/// Recall that signals can fire during llvm_shutdown. If this occurs we should
/// either clean something up or nothing at all, but we shouldn't crash!
struct FilesToRemoveCleanup {
  // Not signal-safe.
  ~FilesToRemoveCleanup() {
    FileToRemoveList *Head = FilesToRemove.exchange(nullptr);
    if (Head)
      delete Head;
  }
};
} // namespace

static StringRef Argv0;

// Signals that represent requested termination. There's no bug or failure, or
// if there is, it's not our direct responsibility. For whatever reason, our
// continued execution is no longer desirable.
static const int IntSigs[] = {
  SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1, SIGUSR2
};

// Signals that represent that we have a bug, and our prompt termination has
// been ordered.
static const int KillSigs[] = {
  SIGILL, SIGTRAP, SIGABRT, SIGFPE, SIGBUS, SIGSEGV, SIGQUIT
#ifdef SIGSYS
  , SIGSYS
#endif
#ifdef SIGXCPU
  , SIGXCPU
#endif
#ifdef SIGXFSZ
  , SIGXFSZ
#endif
#ifdef SIGEMT
  , SIGEMT
#endif
};

static const size_t NumSigs =
    array_lengthof(IntSigs) + array_lengthof(KillSigs) + 1 /* SIGPIPE */;

static std::atomic<unsigned> NumRegisteredSignals = ATOMIC_VAR_INIT(0);
static struct {
  __sa_handler_t SA;
  int SigNo;
} RegisteredSignalInfo[NumSigs];


static void RegisterHandler(int Signal) {
  //unsigned Index = NumRegisteredSignals.load();
  assert(Index < array_lengthof(RegisteredSignalInfo) &&
         "Out of space for signal handlers!");
         
  // Install the new handler, save the old one in RegisteredSignalInfo.
  RegisteredSignalInfo[NumRegisteredSignals].SA = signal(Signal, SignalHandler);
  RegisteredSignalInfo[NumRegisteredSignals].SigNo = Signal;
  ++NumRegisteredSignals;
}

static void RegisterHandlers() {
  // The mutex prevents other threads from registering handlers while we're
  // doing it. We also have to protect the handlers and their count because
  // a signal handler could fire while we're registeting handlers.
  static ManagedStatic<sys::SmartMutex<true>> SignalHandlerRegistrationMutex;
  sys::SmartScopedLock<true> Guard(*SignalHandlerRegistrationMutex);

  // If the handlers are already registered, we're done.
  if (NumRegisteredSignals.load() != 0)
    return;

  for (auto S : IntSigs) RegisterHandler(S);
  for (auto S : KillSigs) RegisterHandler(S);
}

void sys::unregisterHandlers() {
  // Restore all of the signal handlers to how they were before we showed up.
  for (unsigned i = 0, e = NumRegisteredSignals.load(); i != e; ++i) {
    signal(RegisteredSignalInfo[i].SigNo, RegisteredSignalInfo[i].SA);
    --NumRegisteredSignals;
  }
}


/// Process the FilesToRemove list.
static void RemoveFilesToRemove() {
  FileToRemoveList::removeAllFiles(FilesToRemove);
}

void sys::CleanupOnSignal(uintptr_t Context) {
  int Sig = (int)Context;
  RemoveFilesToRemove();

  if (llvm::is_contained(IntSigs, Sig) || Sig == SIGPIPE)
    return;

  llvm::sys::RunSignalHandlers();
}

// The signal handler that runs.
static RETSIGTYPE SignalHandler(int Sig) {
  // Restore the signal behavior to default, so that the program actually
  // crashes when we return and the signal reissues.  This also ensures that if
  // we crash in our signal handler that the program will terminate immediately
  // instead of recursing in the signal handler.
  sys::unregisterHandlers();

  {
    RemoveFilesToRemove();

    if (Sig == SIGPIPE)
      if (auto OldOneShotPipeFunction =
              OneShotPipeSignalFunction.exchange(nullptr))
        return OldOneShotPipeFunction();
    
    bool IsIntSig = llvm::is_contained(IntSigs, Sig);
    if (IsIntSig)
      if (auto OldInterruptFunction = InterruptFunction.exchange(nullptr))
        return OldInterruptFunction();

    if (Sig == SIGPIPE || IsIntSig) {
      raise(Sig); // Execute the default handler.
      return;
    }
  }

  // Otherwise if it is a fault (like SEGV) run any handler.
  llvm::sys::RunSignalHandlers();

#ifdef __s390__
  // On S/390, certain signals are delivered with PSW Address pointing to
  // *after* the faulting instruction.  Simply returning from the signal
  // handler would continue execution after that point, instead of
  // re-raising the signal.  Raise the signal manually in those cases.
  if (Sig == SIGILL || Sig == SIGFPE || Sig == SIGTRAP)
    raise(Sig);
#endif
}

void llvm::sys::RunInterruptHandlers() {
  RemoveFilesToRemove();
}

void llvm::sys::SetInterruptFunction(void (*IF)()) {
  InterruptFunction.exchange(IF);
  RegisterHandlers();
}

void llvm::sys::SetInfoSignalFunction(void (*Handler)()) {
  // Unimplemented.
}

void llvm::sys::SetOneShotPipeSignalFunction(void (*Handler)()) {
  OneShotPipeSignalFunction.exchange(Handler);
  RegisterHandlers();
}

void llvm::sys::DefaultOneShotPipeSignalHandler() {
  // Unimplemented.
}

// The public API
bool llvm::sys::RemoveFileOnSignal(StringRef Filename,
                                   std::string* ErrMsg) {
  // Ensure that cleanup will occur as soon as one file is added.
  static ManagedStatic<FilesToRemoveCleanup> FilesToRemoveCleanup;
  *FilesToRemoveCleanup;
  FileToRemoveList::insert(FilesToRemove, Filename.str());
  RegisterHandlers();
  return false;
}

// The public API
void llvm::sys::DontRemoveFileOnSignal(StringRef Filename) {
  FileToRemoveList::erase(FilesToRemove, Filename.str());
}

/// AddSignalHandler - Add a function to be called when a signal is delivered
/// to the process.  The handler can have a cookie passed to it to identify
/// what instance of the handler it is.
void llvm::sys::AddSignalHandler(void (*FnPtr)(void *), void *Cookie) {
  insertSignalHandler(FnPtr, Cookie);
  RegisterHandlers();
}

/// This platform does not have dl_iterate_phdr, so we do not yet know how to
/// find all loaded DSOs.
static bool findModulesAndOffsets(void **StackTrace, int Depth,
                                  const char **Modules, intptr_t *Offsets,
                                  const char *MainExecutableName,
                                  StringSaver &StrPool) {
  return false;
}

#if ENABLE_BACKTRACES && defined(HAVE__UNWIND_BACKTRACE)
static int unwindBacktrace(void **StackTrace, int MaxEntries) {
  if (MaxEntries < 0)
    return 0;

  // Skip the first frame ('unwindBacktrace' itself).
  int Entries = -1;

  auto HandleFrame = [&](_Unwind_Context *Context) -> _Unwind_Reason_Code {
    // Apparently we need to detect reaching the end of the stack ourselves.
    void *IP = (void *)_Unwind_GetIP(Context);
    if (!IP)
      return _URC_END_OF_STACK;

    assert(Entries < MaxEntries && "recursively called after END_OF_STACK?");
    if (Entries >= 0)
      StackTrace[Entries] = IP;

    if (++Entries == MaxEntries)
      return _URC_END_OF_STACK;
    return _URC_NO_REASON;
  };

  _Unwind_Backtrace(
      [](_Unwind_Context *Context, void *Handler) {
        return (*static_cast<decltype(HandleFrame) *>(Handler))(Context);
      },
      static_cast<void *>(&HandleFrame));
  return std::max(Entries, 0);
}
#endif

// In the case of a program crash or fault, print out a stack trace so that the
// user has an indication of why and where we died.
//
// On glibc systems we have the 'backtrace' function, which works nicely, but
// doesn't demangle symbols.
void llvm::sys::PrintStackTrace(raw_ostream &OS, int Depth) {
#if ENABLE_BACKTRACES
  static void *StackTrace[256];
  int depth = 0;
#if defined(HAVE_BACKTRACE)
  // Use backtrace() to output a backtrace on Linux systems with glibc.
  if (!depth)
    depth = backtrace(StackTrace, static_cast<int>(array_lengthof(StackTrace)));
#endif
#if defined(HAVE__UNWIND_BACKTRACE)
  // Try _Unwind_Backtrace() if backtrace() failed.
  if (!depth)
    depth = unwindBacktrace(StackTrace,
                        static_cast<int>(array_lengthof(StackTrace)));
#endif
  if (!depth)
    return;

  if (printSymbolizedStackTrace(Argv0, StackTrace, depth, OS))
    return;
#if HAVE_DLFCN_H && HAVE_DLADDR
  int width = 0;
  for (int i = 0; i < depth; ++i) {
    Dl_info dlinfo;
    dladdr(StackTrace[i], &dlinfo);
    const char* name = strrchr(dlinfo.dli_fname, '/');

    int nwidth;
    if (!name) nwidth = strlen(dlinfo.dli_fname);
    else       nwidth = strlen(name) - 1;

    if (nwidth > width) width = nwidth;
  }

  for (int i = 0; i < depth; ++i) {
    Dl_info dlinfo;
    dladdr(StackTrace[i], &dlinfo);

    OS << format("%-2d", i);

    const char* name = strrchr(dlinfo.dli_fname, '/');
    if (!name) OS << format(" %-*s", width, dlinfo.dli_fname);
    else       OS << format(" %-*s", width, name+1);

    OS << format(" %#0*lx", (int)(sizeof(void*) * 2) + 2,
                 (unsigned long)StackTrace[i]);

    if (dlinfo.dli_sname != nullptr) {
      OS << ' ';
      int res;
      char* d = itaniumDemangle(dlinfo.dli_sname, nullptr, nullptr, &res);
      if (!d) OS << dlinfo.dli_sname;
      else    OS << d;
      free(d);

      // FIXME: When we move to C++11, use %t length modifier. It's not in
      // C++03 and causes gcc to issue warnings. Losing the upper 32 bits of
      // the stack offset for a stack dump isn't likely to cause any problems.
      OS << format(" + %u",(unsigned)((char*)StackTrace[i]-
                                      (char*)dlinfo.dli_saddr));
    }
    OS << '\n';
  }
#elif defined(HAVE_BACKTRACE)
  backtrace_symbols_fd(StackTrace, depth, STDERR_FILENO);
#endif
#endif
}

static void PrintStackTraceSignalHandler(void *) {
  sys::PrintStackTrace(llvm::errs());
}

void llvm::sys::DisableSystemDialogsOnCrash() {}

/// PrintStackTraceOnErrorSignal - When an error signal (such as SIGABRT or
/// SIGSEGV) is delivered to the process, print a stack trace and then exit.
void llvm::sys::PrintStackTraceOnErrorSignal(StringRef Argv0,
                                             bool DisableCrashReporting) {
  ::Argv0 = Argv0;

  AddSignalHandler(PrintStackTraceSignalHandler, nullptr);
}
