// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Included inside filesystem::detail. Keep Vali's permission layout out of
// filesystem::perms, whose bit values are specified by the C++ standard.
#ifndef PATH_MAX
#  define PATH_MAX _MAXPATH
#endif
using ModeT  = unsigned;
using SSizeT = intptr_t;
struct StatVFS {
  uint64_t f_frsize, f_blocks, f_bfree, f_bavail;
};
inline unsigned vali_permissions(unsigned mode) {
  unsigned result = 0;
  for (unsigned bit = 0; bit != 9; ++bit)
    if (mode & (1u << (8 - bit)))
      result |= 1u << bit;
  return result;
}
inline int vali_result(oserr_t result) { return OsErrToErrNo(result); }
inline void vali_stat(const OSFileDescriptor_t& in, StatT* out) {
  *out         = {};
  out->st_dev  = in.StorageId;
  out->st_ino  = in.Id;
  out->st_size = in.Size.QuadPart;
  out->st_mode = vali_permissions(in.Permissions);
  out->st_mode |=
      FILE_FLAG_TYPE(in.Flags) == FILE_FLAG_DIRECTORY ? S_IFDIR
      : FILE_FLAG_TYPE(in.Flags) == FILE_FLAG_LINK
          ? S_IFLNK
          : S_IFREG;
  out->st_atim = {in.AccessedAt.Seconds + 946684800, in.AccessedAt.Nanoseconds};
  out->st_mtim = {in.ModifiedAt.Seconds + 946684800, in.ModifiedAt.Nanoseconds};
}
inline int stat(const char* p, StatT* out) {
  OSFileDescriptor_t info;
  int result = vali_result(GetFileInformationFromPath(p, 1, &info));
  if (result == 0)
    vali_stat(info, out);
  return result;
}
inline int lstat(const char* p, StatT* out) {
  OSFileDescriptor_t info;
  int result = vali_result(GetFileInformationFromPath(p, 0, &info));
  if (result == 0)
    vali_stat(info, out);
  return result;
}
inline int fstat(int fd, StatT* out) {
  OSFileDescriptor_t info;
  int result = vali_result(GetFileInformationFromFd(fd, &info));
  if (result == 0)
    vali_stat(info, out);
  return result;
}
inline int open(const char* p, int flags, unsigned mode = 0) { return ::open(p, flags, vali_permissions(mode)); }
inline int mkdir(const char* p, unsigned mode) { return ::mkdir(p, vali_permissions(mode)); }
inline int chmod(const char* p, unsigned mode) {
  if (mode & 07000) {
    errno = ENOTSUP;
    return -1;
  }
  return vali_result(ChangeFilePermissionsFromPath(p, vali_permissions(mode)));
}
inline int fchmod(int fd, unsigned mode) {
  if (mode & 07000) {
    errno = ENOTSUP;
    return -1;
  }
  return vali_result(ChangeFilePermissionsFromFd(fd, vali_permissions(mode)));
}
inline int truncate(const char* p, off_t size) {
  if (size < 0 || static_cast<uintmax_t>(size) > SIZE_MAX) {
    errno = EINVAL;
    return -1;
  }
  return vali_result(SetFileSizeFromPath(p, static_cast<size_t>(size)));
}
inline int ftruncate(int fd, off_t size) {
  if (size < 0 || static_cast<uintmax_t>(size) > SIZE_MAX) {
    errno = EINVAL;
    return -1;
  }
  return vali_result(SetFileSizeFromFd(fd, static_cast<size_t>(size)));
}
inline int symlink_file(const char* from, const char* to) { return ::link(from, to, 1); }
inline int symlink_dir(const char* from, const char* to) { return ::link(from, to, 1); }
inline int link(const char* from, const char* to) { return ::link(from, to, 0); }
inline int chdir(const char* p) { return vali_result(OSChangeWorkingDirectory(p)); }
inline char* getcwd(char* buffer, size_t size) {
  bool allocated = buffer == nullptr;
  if (allocated) {
    size   = PATH_MAX;
    buffer = static_cast<char*>(malloc(size));
  }
  if (!buffer) {
    errno = ENOMEM;
    return nullptr;
  }
  if (vali_result(OSGetWorkingDirectory(buffer, size)) != 0) {
    if (allocated)
      free(buffer);
    return nullptr;
  }
  return buffer;
}
inline char* realpath(const char* p, char* buffer) {
  bool allocated = buffer == nullptr;
  if (allocated)
    buffer = static_cast<char*>(malloc(PATH_MAX));
  if (!buffer) {
    errno = ENOMEM;
    return nullptr;
  }
  if (vali_result(OSGetFullPath(p, 1, buffer, PATH_MAX)) != 0) {
    if (allocated)
      free(buffer);
    return nullptr;
  }
  return buffer;
}
inline SSizeT readlink(const char* p, char* buffer, size_t size) {
  char target[PATH_MAX] = {};
  if (vali_result(GetFileLink(p, target, sizeof(target))) != 0)
    return -1;
  size_t length = strnlen(target, sizeof(target));
  if (length >= sizeof(target) - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }
  if (length > size)
    length = size;
  memcpy(buffer, target, length);
  return static_cast<SSizeT>(length);
}
inline int statvfs(const char* p, StatVFS* out) {
  OSFileSystemDescriptor_t info;
  if (vali_result(GetFileSystemInformationFromPath(p, 1, &info)) != 0)
    return -1;
  out->f_frsize = uint64_t(info.BlockSize) * info.BlocksPerSegment;
  out->f_blocks = info.SegmentsTotal.QuadPart;
  out->f_bfree = out->f_bavail = info.SegmentsFree.QuadPart;
  return 0;
}
using ::remove;
using ::rename;
#define S_ISREG(m) (((m) & (S_IFREG | S_IFDIR | S_IFLNK)) == S_IFREG)
#define S_ISDIR(m) (((m) & (S_IFREG | S_IFDIR | S_IFLNK)) == S_IFDIR)
#define S_ISLNK(m) (((m) & (S_IFREG | S_IFDIR | S_IFLNK)) == S_IFLNK)
#define S_ISBLK(m) false
#define S_ISCHR(m) false
#define S_ISFIFO(m) false
#define S_ISSOCK(m) false
