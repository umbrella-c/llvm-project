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
#include <threads.h>
#include <errno.h>
#include <stdio.h>
#include <io.h>

using namespace llvm;

namespace llvm {
namespace sys  {
namespace fs {

const file_t kInvalidFile = -1;

// Implemented
std::string getMainExecutable(const char *argv0, void *MainAddr) {
  return std::string(argv0);
}

// Implemented
UniqueID file_status::getUniqueID() const {
  LargeUInteger FileID;
  FileID.u.LowPart = FileIndexLow;
  FileID.u.HighPart = FileIndexHigh;
  return UniqueID((uint64_t)VolumeSerialNumber, FileID.QuadPart);
}

// Implemented
TimePoint<> basic_file_status::getLastAccessedTime() const {
  LargeUInteger Time;
  Time.u.LowPart = LastAccessedTimeLow;
  Time.u.HighPart = LastAccessedTimeHigh;
  return toTimePoint(Time.QuadPart);
}

// Implemented
TimePoint<> basic_file_status::getLastModificationTime() const {
  LargeUInteger Time;
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
  OsFileSystemDescriptor_t Descriptor;
  if (!::GetFileSystemInformationFromPath(Path.str().c_str(), &Descriptor))
    return make_error_code(errc::no_such_file_or_directory);
  space_info SpaceInfo;
  uint64_t SegmentSizeInBytes = Descriptor.BlockSize * Descriptor.BlocksPerSegment;
  SpaceInfo.capacity          = Descriptor.SegmentsTotal.QuadPart * SegmentSizeInBytes;
  SpaceInfo.free              = Descriptor.SegmentsFree.QuadPart * SegmentSizeInBytes;
  SpaceInfo.available         = SpaceInfo.capacity - SpaceInfo.free;
  return SpaceInfo;
}

// Implemented
std::error_code current_path(SmallVectorImpl<char> &result) {
  SmallVector<char, _MAXPATH> cur_path;
  if (GetWorkingDirectory(cur_path.data(), _MAXPATH) != OsSuccess) {
      return make_error_code(errc::invalid_argument);
  }
  cur_path.set_size(strlen(cur_path.data()) + 1);
  result.clear();
  result.append(cur_path.begin(), cur_path.end());
  return std::error_code();
}

// Implemented
std::error_code set_current_path(const Twine &path) {
  SmallString<_MAXPATH> path_storage;
  StringRef f = path.toNullTerminatedStringRef(path_storage);
  if (SetWorkingDirectory(f.begin()) != OsSuccess) {
      return make_error_code(errc::no_such_file_or_directory);
  }
  return std::error_code();
}

// Implemented
std::error_code create_directory(const Twine &path, bool IgnoreExisting,
                                 perms Perms) {
  SmallString<_MAXPATH> path_storage;
  struct DIR *dhandle = NULL;
  StringRef f = path.toNullTerminatedStringRef(path_storage);
  int ec = ::opendir(f.begin(), O_CREAT | O_EXCL, &dhandle);
  if (ec == -1) {
      if (errno == EACCES) {
          return make_error_code(errc::permission_denied);
      }
      if (errno == EEXIST) {
          return IgnoreExisting ? std::error_code() : make_error_code(errc::file_exists);
      }
      return std::error_code(errno, std::generic_category());
  }
  ::closedir(dhandle);
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
  OsStorageDescriptor_t Descriptor;
  SmallString<_MAXPATH> path_storage;
  StringRef f = path.toNullTerminatedStringRef(path_storage);
  if (GetStorageInformationFromPath(f.begin(), &Descriptor) != OsSuccess) {
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
  OsStorageDescriptor_t Descriptor;
  if (GetStorageInformationFromFd(FD, &Descriptor) != OsSuccess) {
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
    OsFileDescriptor_t Descriptor;
    SmallString<256> path_storage;
    StringRef f = Path.toNullTerminatedStringRef(path_storage);
    if (GetFileInformationFromPath(f.begin(), &Descriptor) != OsSuccess) {
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
    OsFileDescriptor_t Descriptor;
    SmallString<256> path_storage;
    StringRef f = path.toNullTerminatedStringRef(path_storage);
    if (GetFileInformationFromPath(f.begin(), &Descriptor) != OsSuccess) {
        return errc::no_such_file_or_directory;
    }

    LargeUInteger Accessed, Modified, FileID;
    Accessed.QuadPart = Descriptor.AccessedAt.tv_sec;
    Modified.QuadPart = Descriptor.ModifiedAt.tv_sec;
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
    OsFileDescriptor_t Descriptor;
    if (GetFileInformationFromFd(FD, &Descriptor) != OsSuccess) {
        return errc::no_such_file_or_directory;
    }
    
    LargeUInteger Accessed, Modified, FileID;
    Accessed.QuadPart = Descriptor.AccessedAt.tv_sec;
    Modified.QuadPart = Descriptor.ModifiedAt.tv_sec;
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
  OsStatus_t       Status;
  int              Access = FILE_PERMISSION_READ;
  SmallString<128> PathStorage;
  StringRef        P = Path.toNullTerminatedStringRef(PathStorage);

  if (Permissions & 0222)
    Access |= FILE_PERMISSION_WRITE;

  Status = ChangeFilePermissionsFromPath(P.begin(), Access);
  if (OsStatusToErrno(Status)) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

std::error_code setPermissions(int FD, perms Permissions) {
  OsStatus_t Status;
  int        Access = FILE_PERMISSION_READ;

  if (Permissions & 0222)
    Access |= FILE_PERMISSION_WRITE;

  Status = ChangeFilePermissionsFromFd(FD, Access);
  if (OsStatusToErrno(Status)) {
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
  int Flags = (Mode == readonly) ? FILE_MAPPING_READ : (FILE_MAPPING_READ | FILE_MAPPING_WRITE);
  this->Mode = Mode;
  assert(Size != 0);
  if (::CreateFileMapping(FD, Flags, Offset, Size, &Mapping) != OsSuccess)
    return std::error_code(errno, std::generic_category());
  return std::error_code();
}

// Implemented
mapped_file_region::mapped_file_region(int fd, mapmode mode, size_t length,
                                       uint64_t offset, std::error_code &ec)
    : Size(length), Mapping() {
  ec = init(fd, offset, mode);
  if (ec)
    Mapping = nullptr;
}

// Implemented
void mapped_file_region::unmapImpl() {
  if (Mapping)
    ::DestroyFileMapping(Mapping);
}

// Implemented
int mapped_file_region::alignment() {
  SystemDescriptor_t Descriptor;
  if (SystemQuery(&Descriptor) != OsSuccess) {
      // Default to 0x1000
      return 0x1000;
  }
  return Descriptor.PageSizeBytes;
}

// Implemented
std::error_code detail::directory_iterator_construct(detail::DirIterState &it,
                                                     StringRef path,
                                                     bool follow_symlinks) {
  SmallString<128> path_null(path);
  struct DIR *directory = nullptr; 
  int ec = ::opendir(path_null.c_str(), O_RDONLY, &directory);
  if (!directory || ec) {
      return std::error_code(ec, std::generic_category());
  }

  it.IterationHandle = reinterpret_cast<intptr_t>(directory);
  // Add something for replace_filename to replace.
  path::append(path_null, ".");
  it.CurrentEntry = directory_entry(path_null.str(), follow_symlinks);
  return directory_iterator_increment(it);
}

// Implemented
std::error_code detail::directory_iterator_destruct(detail::DirIterState &it) {
  if (it.IterationHandle)
    ::closedir(reinterpret_cast<struct DIR*>(it.IterationHandle));
  it.IterationHandle = 0;
  it.CurrentEntry = directory_entry();
  return std::error_code();
}

static file_type direntType(struct DIRENT* Entry) {
  if (Entry->d_options & FILE_FLAG_DIRECTORY) {
      return file_type::directory_file;
  }
  else if (Entry->d_options & FILE_FLAG_LINK) {
      return file_type::symlink_file;
  }
  return file_type::regular_file;
}

// Implemented
std::error_code detail::directory_iterator_increment(detail::DirIterState &it) {
  struct DIRENT dentry;
  errno = 0;
  int ec = ::readdir(reinterpret_cast<struct DIR*>(it.IterationHandle), &dentry);
  if (ec != EOK && ec != EOF) {
    return std::error_code(ec, std::generic_category());
  } else if (ec != EOF) {
    StringRef name(&dentry.d_name[0], strlen(&dentry.d_name[0]));
    if ((name.size() == 1 && name[0] == '.') ||
        (name.size() == 2 && name[0] == '.' && name[1] == '.'))
      return directory_iterator_increment(it);
    it.CurrentEntry.replace_filename(name, direntType(&dentry));
  } else
    return directory_iterator_destruct(it);

  return std::error_code();
}

// Implemented
ErrorOr<basic_file_status> directory_entry::status() const {
  return Status;
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

std::error_code tryLockFile(int FD, std::chrono::milliseconds Timeout) {
  auto Start = std::chrono::steady_clock::now();
  auto End = Start + Timeout;
  do {
    if (!::iolock(FD))
      return std::error_code();
    int Error = errno;
    if (Error != EACCES && Error != EAGAIN)
      return std::error_code(Error, std::generic_category());
    thrd_sleepex(1);
  } while (std::chrono::steady_clock::now() < End);
  return make_error_code(errc::no_lock_available);
}

std::error_code lockFile(int FD) {
  if (!::iolock(FD))
    return std::error_code();
  int Error = errno;
  return std::error_code(Error, std::generic_category());
}

std::error_code unlockFile(int FD) {
  if (!::iounlock(FD))
    return std::error_code();
  return std::error_code(errno, std::generic_category());
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
    SmallString<256> name_storage;
    StringRef f = Name.toNullTerminatedStringRef(name_storage);
    // @todo handle openflags

    int fd = open(f.begin(), O_RDONLY);
    if (fd == -1) {
        return std::error_code(errno, std::generic_category());
    }
    ResultFD = fd;
    if (RealPath) {
        RealPath->clear();
        RealPath->reserve(_MAXPATH);
        if (GetFilePathFromFd(fd, (char*)RealPath->begin(), _MAXPATH) != OsSuccess) {
            return std::error_code(errno, std::generic_category());
        }
        RealPath->set_size(strlen(RealPath->begin()) + 1);
    }
    return std::error_code();
}

// Implemented
std::error_code openFileForWrite(const Twine &Name, int &ResultFD,
                            sys::fs::OpenFlags Flags, unsigned Mode) {
  // Verify that we don't have both "append" and "excl". @todo
  SmallString<256> name_storage;
  StringRef f = Name.toNullTerminatedStringRef(name_storage);
  int oflags = O_RDWR;

  // Resolve Mode flags @todo

  // Resolve creation flags
  if (Flags & OF_Append) { oflags |= O_APPEND; }
  else                   { oflags |= O_CREAT;  }
  if (Flags & OF_Delete) { oflags |= O_TEMPORARY; }
  if (Flags & OF_Text)   { oflags |= O_TEXT; }

  // open
  int fd = open(f.begin(), oflags);
  if (fd == -1) {
      return std::error_code(errno, std::generic_category());
  }
  ResultFD = fd;
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

// Implemented
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

  if (is_directory(path)) {
    SmallString<256> path_storage;
    struct DIR *dhandle = NULL;
    StringRef f = path.toNullTerminatedStringRef(path_storage);
    dest.reserve(_MAXPATH);
    if (::opendir(f.begin(), O_RDONLY, &dhandle) == -1) {
        return std::error_code(errno, std::generic_category());
    }
    //@todo
    //GetFilePathFromHandle(dhandle->d_handle, (const char*)dest.begin(), _MAXPATH);
    dest.set_size(strlen((const char*)dest.begin()) + 1);
    ::closedir(dhandle);
    return std::error_code();
  }

  int fd;
  if (std::error_code EC = llvm::sys::fs::openFileForRead(path, fd, OF_None, &dest))
    return EC;
  ::close(fd);
  return std::error_code();
}

} // end namespace fs


// Implemented
namespace path {
bool getUserCacheDir(SmallVectorImpl<char> &Result) {
    SmallVector<char, _MAXPATH> Buf;
    if (GetUserCacheDirectory(Buf.data(), Buf.capacity()) != OsSuccess) {
        return false;
    }
    Buf.set_size(strlen(Buf.data()) + 1);
    Result.append(Buf.begin(), Buf.end());
    return true;
}

bool home_directory(SmallVectorImpl<char> &result) {
    SmallVector<char, _MAXPATH> Buf;
    if (GetUserDirectory(Buf.data(), Buf.capacity()) != OsSuccess) {
        return false;
    }
    Buf.set_size(strlen(Buf.data()) + 1);
    result.append(Buf.begin(), Buf.end());
    return true;
}

void system_temp_directory(bool ErasedOnReboot, SmallVectorImpl<char> &Result) {
    SmallVector<char, _MAXPATH> Buf;
    Result.clear();
    if (ErasedOnReboot) {
        if (GetApplicationTemporaryDirectory(Buf.data(), Buf.capacity()) != OsSuccess) {
            return;
        }
    }
    else {
        if (GetApplicationDirectory(Buf.data(), Buf.capacity()) != OsSuccess) {
            return;
        }
    }
    Buf.set_size(strlen(Buf.data()) + 1);
    Result.append(Buf.begin(), Buf.end());
}
} // end namespace path
} // end namespace sys
} // end namespace llvm
