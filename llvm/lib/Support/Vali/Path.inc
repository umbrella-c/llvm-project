//===- llvm/Support/Windows/Path.inc - Windows Path Impl --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Windows specific implementation of the Path API.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include <os/mollenos.h>
#include <os/services/file.h>
#include <os/services/process.h>
#include <os/services/path.h>
#include <sys/mman.h>
#include <threads.h>
#include <errno.h>
#include <stdio.h>
#include <io.h>

using namespace llvm;

namespace llvm {
namespace sys  {
namespace fs {

const file_t kInvalidFile = -1;

/// GetMainExecutable - Return the path to the main executable, given the
/// value of argv[0] from program startup.
std::string getMainExecutableImpl(const char *argv0, void *MainAddr) {
  return std::string(argv0);
}

// Implemented
UniqueID file_status::getUniqueID() const {
  UInteger64_t FileID;
  FileID.u.LowPart = FileIndexLow;
  FileID.u.HighPart = FileIndexHigh;
  return UniqueID((uint64_t)VolumeSerialNumber, FileID.QuadPart);
}

// Implemented
TimePoint<> basic_file_status::getLastAccessedTime() const {
  UInteger64_t Time;
  Time.u.LowPart = LastAccessedTimeLow;
  Time.u.HighPart = LastAccessedTimeHigh;
  return toTimePoint(Time.QuadPart);
}

// Implemented
TimePoint<> basic_file_status::getLastModificationTime() const {
  UInteger64_t Time;
  Time.u.LowPart = LastWriteTimeLow;
  Time.u.HighPart = LastWriteTimeHigh;
  return toTimePoint(Time.QuadPart);
}

// Implemented
uint32_t file_status::getLinkCount() const {
  return NumLinks;
}

file_t getStdinHandle() { return STDIN_FILENO; }
file_t getStdoutHandle() { return STDOUT_FILENO; }
file_t getStderrHandle() { return STDERR_FILENO; }

// Implemented
ErrorOr<space_info> disk_space(const Twine &Path) {
  OSFileSystemDescriptor_t Descriptor;
  if (!::GetFileSystemInformationFromPath(Path.str().c_str(), 1, &Descriptor))
    return make_error_code(errc::no_such_file_or_directory);
  space_info SpaceInfo;
  uint64_t SegmentSizeInBytes = Descriptor.BlockSize * Descriptor.BlocksPerSegment;
  SpaceInfo.capacity          = Descriptor.SegmentsTotal.QuadPart * SegmentSizeInBytes;
  SpaceInfo.free              = Descriptor.SegmentsFree.QuadPart * SegmentSizeInBytes;
  SpaceInfo.available         = SpaceInfo.capacity - SpaceInfo.free;
  return SpaceInfo;
}

std::error_code current_path(SmallVectorImpl<char> &result) {
  result.clear();
  result.resize_for_overwrite(_MAXPATH);

  if (OSProcessWorkingDirectory(UUID_INVALID, result.data(), result.size()) != OS_EOK) {
      return make_error_code(errc::invalid_argument);
  }
  return std::error_code();
}

std::error_code set_current_path(const Twine &path) {
  SmallString<128> path_storage;
  StringRef f = path.toNullTerminatedStringRef(path_storage);
  if (OSProcessSetWorkingDirectory(f.begin()) != OS_EOK) {
      return make_error_code(errc::no_such_file_or_directory);
  }
  return std::error_code();
}

std::error_code create_directory(const Twine &path, bool IgnoreExisting,
                                 perms Perms) {
  SmallString<128> path_storage;
  StringRef p = path.toNullTerminatedStringRef(path_storage);

  if (::mkdir(p.begin(), Perms) == -1) {
    if (errno != EEXIST || !IgnoreExisting)
      return std::error_code(errno, std::generic_category());
  }

  return std::error_code();
}

// Implemented
std::error_code create_link(const Twine &to, const Twine &from) {
  // Get arguments.
  SmallString<256> from_storage;
  SmallString<256> to_storage;
  StringRef f = from.toNullTerminatedStringRef(from_storage);
  StringRef t = to.toNullTerminatedStringRef(to_storage);
  if (::link(f.begin(), t.begin(), 1) != 0) {
      return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

// Implemented
std::error_code create_hard_link(const Twine &to, const Twine &from) {
  // Get arguments.
  SmallString<256> from_storage;
  SmallString<256> to_storage;
  StringRef f = from.toNullTerminatedStringRef(from_storage);
  StringRef t = to.toNullTerminatedStringRef(to_storage);
  if (::link(f.begin(), t.begin(), 0) != 0) {
      return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

// Implemented
std::error_code remove(const Twine &path, bool IgnoreNonExisting) {
  SmallString<_MAXPATH> path_storage;
  StringRef f = path.toNullTerminatedStringRef(path_storage);
    if (!::remove(f.begin())) {
        return std::error_code();
    }
    if (IgnoreNonExisting && errno == ENOENT) {
        return std::error_code();
    }
    return std::error_code(errno, std::generic_category());
}

// Implemented
std::error_code is_local(const Twine &path, bool &result) {
  if (!llvm::sys::fs::exists(path) || !llvm::sys::path::has_root_path(path))
    return make_error_code(errc::no_such_file_or_directory);
  OSStorageDescriptor_t Descriptor;
  SmallString<_MAXPATH> path_storage;
  StringRef f = path.toNullTerminatedStringRef(path_storage);
  if (GetStorageInformationFromPath(f.begin(), 1, &Descriptor) != OS_EOK) {
    return make_error_code(errc::no_such_file_or_directory);
  }
  if (Descriptor.Flags & STORAGE_STATIC) {
    result = true;
    return std::error_code();
  }
  result = false;
  return std::error_code();
}

// Implemented
std::error_code is_local(int FD, bool &Result) {
  OSStorageDescriptor_t Descriptor;
  if (GetStorageInformationFromFd(FD, &Descriptor) != OS_EOK) {
    return make_error_code(errc::no_such_file_or_directory);
  }
  if (Descriptor.Flags & STORAGE_STATIC) {
    Result = true;
    return std::error_code();
  }
  Result = false;
  return std::error_code();
}

// Implemented
std::error_code rename(const Twine &From, const Twine &To) {
  // Get arguments.
  SmallString<256> from_storage;
  SmallString<256> to_storage;
  StringRef f = From.toNullTerminatedStringRef(from_storage);
  StringRef t = To.toNullTerminatedStringRef(to_storage);
  if (::rename(f.begin(), t.begin()) == -1)
    return std::error_code(errno, std::generic_category());
  return std::error_code();
}

// Implemented
std::error_code resize_file(int FD, uint64_t Size) {
  ::chsize(FD, Size);
  return std::error_code(errno, std::generic_category());
}

// Implemented
std::error_code access(const Twine &Path, AccessMode Mode) {
    OSFileDescriptor_t Descriptor;
    SmallString<256> path_storage;
    StringRef f = Path.toNullTerminatedStringRef(path_storage);
    if (GetFileInformationFromPath(f.begin(), 1, &Descriptor) != OS_EOK) {
        return errc::no_such_file_or_directory;
    }
    if (Mode == AccessMode::Write && !(Descriptor.Permissions & FILE_PERMISSION_WRITE)) {
        return errc::permission_denied;
    }
    if (Mode == AccessMode::Execute && !(Descriptor.Permissions & FILE_PERMISSION_EXECUTE)) {
        return errc::permission_denied;
    }
    return std::error_code();
}

// Implemented
bool can_execute(const Twine &Path) {
  return !access(Path, AccessMode::Execute) ||
         !access(Path + ".run", AccessMode::Execute);
}

// Implemented
bool equivalent(file_status A, file_status B) {
  assert(status_known(A) && status_known(B));
  return A.FileSizeHigh          == B.FileSizeHigh &&
         A.FileSizeLow           == B.FileSizeLow &&
         A.LastAccessedTimeHigh  == B.LastAccessedTimeHigh &&
         A.LastAccessedTimeLow   == B.LastAccessedTimeLow &&
         A.LastWriteTimeHigh     == B.LastWriteTimeHigh &&
         A.LastWriteTimeLow      == B.LastWriteTimeLow &&
         A.VolumeSerialNumber    == B.VolumeSerialNumber &&
         A.FileIndexHigh         == B.FileIndexHigh &&
         A.FileIndexLow          == B.FileIndexLow;
}

// Implemented
std::error_code equivalent(const Twine &A, const Twine &B, bool &result) {
  file_status fsA, fsB;
  if (std::error_code ec = status(A, fsA))
    return ec;
  if (std::error_code ec = status(B, fsB))
    return ec;
  result = equivalent(fsA, fsB);
  return std::error_code();
}

// Implemented
static file_type file_type_from_flags(unsigned int Flags) {
    return (Flags & FILE_FLAG_DIRECTORY) ? file_type::directory_file : file_type::regular_file;
}

// Implemented
static perms perms_from_flags(unsigned int Permissions) {
    perms result;
    if (Permissions & FILE_PERMISSION_READ) {
        result = all_read | all_exe;
    }
    if (Permissions & (FILE_PERMISSION_WRITE | FILE_PERMISSION_EXECUTE)) {
        result = all_all;
    }
    return result;
}

// Implemented
std::error_code status(const Twine &path, file_status &result, bool Follow) {
    OSFileDescriptor_t Descriptor;
    SmallString<256> path_storage;
    StringRef f = path.toNullTerminatedStringRef(path_storage);
    if (GetFileInformationFromPath(f.begin(), (int)Follow, &Descriptor) != OS_EOK) {
        return errc::no_such_file_or_directory;
    }

    UInteger64_t Accessed, Modified, FileID;
    Accessed.QuadPart = Descriptor.AccessedAt.Seconds;
    Modified.QuadPart = Descriptor.ModifiedAt.Seconds;
    FileID.QuadPart   = Descriptor.Id;
    result = file_status(
        file_type_from_flags(Descriptor.Flags), 
        perms_from_flags(Descriptor.Permissions), 
        1, 
        Accessed.u.HighPart, Accessed.u.LowPart, 
        Modified.u.HighPart, Modified.u.LowPart, 
        Descriptor.StorageId,
        Descriptor.Size.u.HighPart, Descriptor.Size.u.LowPart,
        FileID.u.HighPart, FileID.u.LowPart
    );
    return std::error_code();
}

// Implemented
std::error_code status(int FD, file_status &Result) {
    OSFileDescriptor_t Descriptor;
    if (GetFileInformationFromFd(FD, &Descriptor) != OS_EOK) {
        return errc::no_such_file_or_directory;
    }
    
    UInteger64_t Accessed, Modified, FileID;
    Accessed.QuadPart = Descriptor.AccessedAt.Seconds;
    Modified.QuadPart = Descriptor.ModifiedAt.Seconds;
    FileID.QuadPart   = Descriptor.Id;
    Result = file_status(
        file_type_from_flags(Descriptor.Flags), perms_from_flags(Descriptor.Permissions), 
        1, 
        Accessed.u.HighPart, Accessed.u.LowPart, 
        Modified.u.HighPart, Modified.u.LowPart, 
        Descriptor.StorageId,
        Descriptor.Size.u.HighPart, Descriptor.Size.u.LowPart,
        FileID.u.HighPart, FileID.u.LowPart
    );
    return std::error_code();
}

unsigned getUmask() {
  return 0;
}

std::error_code setPermissions(const Twine &Path, perms Permissions) {
  oserr_t          Status;
  SmallString<128> PathStorage;
  StringRef        P = Path.toNullTerminatedStringRef(PathStorage);

  Status = ChangeFilePermissionsFromPath(P.begin(), Permissions);
  if (OsErrToErrNo(Status)) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

std::error_code setPermissions(int FD, perms Permissions) {
  oserr_t Status = ChangeFilePermissionsFromFd(FD, Permissions);
  if (OsErrToErrNo(Status)) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

// @todo
std::error_code setLastAccessAndModificationTime(int FD, TimePoint<> AccessTime,
                                                 TimePoint<> ModificationTime) {
  //if (::futimens(FD, Times))
    //return std::error_code(errno, std::generic_category());
  //return std::error_code();
  return make_error_code(errc::function_not_supported);
}

// Implemented
std::error_code mapped_file_region::init(int FD, uint64_t Offset, mapmode Mode) {
  assert(Size != 0);

  int flags = (Mode == readwrite) ? MAP_SHARED : MAP_PRIVATE;
  int prot = (Mode == readonly) ? PROT_READ : (PROT_READ | PROT_WRITE);
#if defined(MAP_NORESERVE)
  flags |= MAP_NORESERVE;
#endif
  Mapping = ::mmap(nullptr, Size, prot, flags, FD, Offset);
  if (Mapping == nullptr)
    return std::error_code(errno, std::generic_category());
  return std::error_code();
}

// Implemented
mapped_file_region::mapped_file_region(int fd, mapmode mode, size_t length,
                                       uint64_t offset, std::error_code &ec)
    : Size(length), Mode(mode) {
  (void)Mode;
  ec = init(fd, offset, mode);
  if (ec)
    copyFrom(mapped_file_region());
}

// Implemented
void mapped_file_region::unmapImpl() {
  if (Mapping)
    ::munmap(Mapping, Size);
}

void mapped_file_region::dontNeedImpl() {
  assert(Mode == mapped_file_region::readonly);
  if (!Mapping)
    return;
  // we have no support for madvise, so no-op this
}

// Implemented
int mapped_file_region::alignment() {
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

std::error_code detail::directory_iterator_construct(detail::DirIterState &it,
                                                     StringRef path,
                                                     bool follow_symlinks) {
  SmallString<128> path_null(path);
  DIR *directory = ::opendir(path_null.c_str());
  if (!directory)
    return std::error_code(errno, std::generic_category());

  it.IterationHandle = reinterpret_cast<intptr_t>(directory);
  // Add something for replace_filename to replace.
  path::append(path_null, ".");
  it.CurrentEntry = directory_entry(path_null.str(), follow_symlinks);
  return directory_iterator_increment(it);
}

std::error_code detail::directory_iterator_destruct(detail::DirIterState &it) {
  if (it.IterationHandle)
    ::closedir(reinterpret_cast<DIR *>(it.IterationHandle));
  it.IterationHandle = 0;
  it.CurrentEntry = directory_entry();
  return std::error_code();
}

static file_type direntType(dirent* Entry) {
  switch (Entry->d_type) {
  case DT_BLK:
    return file_type::block_file;
  case DT_CHR:
    return file_type::character_file;
  case DT_DIR:
    return file_type::directory_file;
  case DT_FIFO:
    return file_type::fifo_file;
  case DT_LNK:
    return file_type::symlink_file;
  case DT_REG:
    return file_type::regular_file;
  case DT_SOCK:
    return file_type::socket_file;
  // Unlike in lstat, hitting "unknown" here simply means that the underlying
  // filesystem doesn't support d_type. Report is as 'none' so we correctly
  // set the cache to empty.
  case DT_UNKNOWN:
    break;
  }
  return file_type::type_unknown;
}

std::error_code detail::directory_iterator_increment(detail::DirIterState &It) {
  errno = 0;
  dirent *CurDir = ::readdir(reinterpret_cast<DIR *>(It.IterationHandle));
  if (CurDir == nullptr && errno != 0) {
    return std::error_code(errno, std::generic_category());
  } else if (CurDir != nullptr) {
    StringRef Name(CurDir->d_name);
    if ((Name.size() == 1 && Name[0] == '.') ||
        (Name.size() == 2 && Name[0] == '.' && Name[1] == '.'))
      return directory_iterator_increment(It);
    It.CurrentEntry.replace_filename(Name, direntType(CurDir));
  } else
    return directory_iterator_destruct(It);

  return std::error_code();
}

ErrorOr<basic_file_status> directory_entry::status() const {
  file_status s;
  if (auto EC = fs::status(Path, s, FollowSymlinks))
    return EC;
  return s;
}

static int nativeOpenFlags(CreationDisposition Disp, OpenFlags Flags,
                           FileAccess Access) {
  int Result = 0;
  if (Access == FA_Read)
    Result |= O_RDONLY;
  else if (Access == FA_Write)
    Result |= O_WRONLY;
  else if (Access == (FA_Read | FA_Write))
    Result |= O_RDWR;

  // This is for compatibility with old code that assumed F_Append implied
  // would open an existing file.  See Windows/Path.inc for a longer comment.
  if (Flags & OF_Append)
    Disp = CD_OpenAlways;

  if (Disp == CD_CreateNew) {
    Result |= O_CREAT; // Create if it doesn't exist.
    Result |= O_EXCL;  // Fail if it does.
  } else if (Disp == CD_CreateAlways) {
    Result |= O_CREAT; // Create if it doesn't exist.
    Result |= O_TRUNC; // Truncate if it does.
  } else if (Disp == CD_OpenAlways) {
    Result |= O_CREAT; // Create if it doesn't exist.
  } else if (Disp == CD_OpenExisting) {
    // Nothing special, just don't add O_CREAT and we get these semantics.
  }

  if (Flags & OF_Append)
    Result |= O_APPEND;

  return Result;
}

// Implemented
std::error_code openFile(const Twine &Name, int &ResultFD,
                         CreationDisposition Disp, FileAccess Access,
                         OpenFlags Flags, unsigned Mode) {
  int OpenFlags = nativeOpenFlags(Disp, Flags, Access);

  SmallString<128> Storage;
  StringRef P = Name.toNullTerminatedStringRef(Storage);
  if ((ResultFD = ::open(P.begin(), OpenFlags, Mode) < 0))
    return std::error_code(errno, std::generic_category());
  return std::error_code();
}

// Implemented
Expected<int> openNativeFile(const Twine &Name, CreationDisposition Disp,
                             FileAccess Access, OpenFlags Flags,
                             unsigned Mode) {

  int FD;
  std::error_code EC = openFile(Name, FD, Disp, Access, Flags, Mode);
  if (EC)
    return errorCodeToError(EC);
  return FD;
}

Expected<file_t> openNativeFileForRead(const Twine &Name, OpenFlags Flags,
                                       SmallVectorImpl<char> *RealPath) {
  file_t ResultFD;
  std::error_code EC = openFileForRead(Name, ResultFD, Flags, RealPath);
  if (EC)
    return errorCodeToError(EC);
  return ResultFD;
}

Expected<size_t> readNativeFile(file_t FD, MutableArrayRef<char> Buf) {
  size_t Size = Buf.size();
  ssize_t NumRead = read(FD, Buf.data(), Size);
  if (ssize_t(NumRead) == -1)
    return errorCodeToError(std::error_code(errno, std::generic_category()));
  return NumRead;
}

Expected<size_t> readNativeFileSlice(file_t FD, MutableArrayRef<char> Buf,
                                     uint64_t Offset) {
  if (lseek(FD, Offset, SEEK_SET) == -1)
    return errorCodeToError(std::error_code(errno, std::generic_category()));
  return readNativeFile(FD, Buf);
}

// TODO: no native lock support
std::error_code tryLockFile(int FD, std::chrono::milliseconds Timeout) {
  return std::error_code();
}

// TODO: no native lock support
std::error_code lockFile(int FD) {
  return std::error_code();
}

// TODO: no native lock support
std::error_code unlockFile(int FD) {
  return std::error_code();
}

std::error_code closeFile(file_t &F) {
  file_t TmpF = F;
  F = kInvalidFile;
  return Process::SafelyCloseFileDescriptor(TmpF);
}

// Implemented
std::error_code openFileForRead(const Twine &Name, int &ResultFD,
                                OpenFlags Flags,
                                SmallVectorImpl<char> *RealPath) {
  std::error_code EC =
      openFile(Name, ResultFD, CD_OpenExisting, FA_Read, Flags, 0666);
  if (EC)
    return EC;

  // Attempt to get the real name of the file, if the user asked
  if (!RealPath)
    return std::error_code();
  RealPath->clear();

  SmallString<128> Storage;
  StringRef P = Name.toNullTerminatedStringRef(Storage);

  // Use ::realpath to get the real path name
  char Buffer[_MAXPATH];
  if (::OSGetFullPath(P.begin(), 1, Buffer, _MAXPATH) == OS_EOK)
    RealPath->append(Buffer, Buffer + strlen(Buffer));
  return std::error_code();
}

// Implemented
template <typename T>
static std::error_code remove_directories_impl(const T &Entry,
                                               bool IgnoreErrors) {
  std::error_code EC;
  directory_iterator Begin(Entry, EC, false);
  directory_iterator End;
  while (Begin != End) {
    auto &Item = *Begin;
    ErrorOr<basic_file_status> st = Item.status();
    if (!st && !IgnoreErrors)
      return st.getError();

    if (is_directory(*st)) {
      EC = remove_directories_impl(Item, IgnoreErrors);
      if (EC && !IgnoreErrors)
        return EC;
    }

    EC = fs::remove(Item.path(), true);
    if (EC && !IgnoreErrors)
      return EC;

    Begin.increment(EC);
    if (EC && !IgnoreErrors)
      return EC;
  }
  return std::error_code();
}

// Implemented
std::error_code remove_directories(const Twine &path, bool IgnoreErrors) {
  auto EC = remove_directories_impl(path, IgnoreErrors);
  if (EC && !IgnoreErrors)
    return EC;
  EC = fs::remove(path, true);
  if (EC && !IgnoreErrors)
    return EC;
  return std::error_code();
}

// Implemented
static void expandTildeExpr(SmallVectorImpl<char> &Path) {
  // Path does not begin with a tilde expression.
  if (Path.empty() || Path[0] != '~')
    return;

  StringRef PathStr(Path.begin(), Path.size());
  PathStr = PathStr.drop_front();
  StringRef Expr = PathStr.take_until([](char c) { return path::is_separator(c); });

  if (!Expr.empty()) {
    // This is probably a ~username/ expression. no support
    return;
  }

  SmallString<128> HomeDir;
  if (!path::home_directory(HomeDir)) {
    // For some reason we couldn't get the home directory.  Just exit.
    return;
  }

  // Overwrite the first character and insert the rest.
  Path[0] = HomeDir[0];
  Path.insert(Path.begin() + 1, HomeDir.begin() + 1, HomeDir.end());
}

void expand_tilde(const Twine &path, SmallVectorImpl<char> &dest) {
  dest.clear();
  if (path.isTriviallyEmpty())
    return;

  path.toVector(dest);
  expandTildeExpr(dest);
}

std::error_code real_path(const Twine &path, SmallVectorImpl<char> &dest,
                          bool expand_tilde) {
  dest.clear();
  if (path.isTriviallyEmpty())
    return std::error_code();

  if (expand_tilde) {
    SmallString<128> Storage;
    path.toVector(Storage);
    expandTildeExpr(Storage);
    return real_path(Storage, dest, false);
  }

  SmallString<128> Storage;
  StringRef P = path.toNullTerminatedStringRef(Storage);
  char Buffer[_MAXPATH];
  if (::OSGetFullPath(P.begin(), 1, Buffer, _MAXPATH) != OS_EOK)
    return std::error_code(errno, std::generic_category());
  dest.append(Buffer, Buffer + strlen(Buffer));
  return std::error_code();
}

} // end namespace fs


// Implemented
namespace path {
bool home_directory(SmallVectorImpl<char> &result) {
  SmallVector<char, _MAXPATH> Buf;
  if (OSGetUserDirectory(Buf.data(), Buf.capacity()) != OS_EOK) {
      return false;
  }
  result.clear();
  result.append(Buf.begin(), Buf.end());
  return true;
}

bool user_config_directory(SmallVectorImpl<char> &result) {
  // Fallback: ~/.config
  if (!home_directory(result)) {
    return false;
  }
  append(result, ".config");
  return true;
}

bool cache_directory(SmallVectorImpl<char> &result) {
  SmallVector<char, _MAXPATH> Buf;
  if (OSGetUserCacheDirectory(Buf.data(), Buf.capacity()) != OS_EOK) {
      return false;
  }
  result.clear();
  result.append(Buf.begin(), Buf.end());
  return true;
}

void system_temp_directory(bool ErasedOnReboot, SmallVectorImpl<char> &Result) {
  SmallVector<char, _MAXPATH> Buf;
  if (ErasedOnReboot) {
      if (OSGetApplicationTemporaryDirectory(Buf.data(), Buf.capacity()) != OS_EOK) {
          return;
      }
  }
  else {
      if (OSGetApplicationDirectory(Buf.data(), Buf.capacity()) != OS_EOK) {
          return;
      }
  }
  Result.clear();
  Result.append(Buf.begin(), Buf.end());
}
} // end namespace path
} // end namespace sys
} // end namespace llvm
