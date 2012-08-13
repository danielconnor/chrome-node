/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <malloc.h>
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <stdio.h>

#include "uv.h"
#include "internal.h"
#include "req-inl.h"


#define UV_FS_ASYNC_QUEUED       0x0001
#define UV_FS_FREE_PATH          0x0002
#define UV_FS_FREE_NEW_PATH      0x0004
#define UV_FS_FREE_PTR           0x0008
#define UV_FS_CLEANEDUP          0x0010


#define UTF8_TO_UTF16(s, t)                                                 \
  size = uv_utf8_to_utf16(s, NULL, 0) * sizeof(wchar_t);                    \
  t = (wchar_t*)malloc(size);                                               \
  if (!t) {                                                                 \
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");                            \
  }                                                                         \
  if (!uv_utf8_to_utf16(s, t, size / sizeof(wchar_t))) {                    \
    uv__set_sys_error(loop, GetLastError());                                \
    return -1;                                                              \
  }

#define QUEUE_FS_TP_JOB(loop, req)                                          \
  if (!QueueUserWorkItem(&uv_fs_thread_proc,                                \
                         req,                                               \
                         WT_EXECUTEDEFAULT)) {                              \
    uv__set_sys_error((loop), GetLastError());                              \
    return -1;                                                              \
  }                                                                         \
  req->flags |= UV_FS_ASYNC_QUEUED;                                         \
  uv__req_register(loop, req);

#define SET_UV_LAST_ERROR_FROM_REQ(req)                                     \
  uv__set_error(req->loop, req->errorno, req->sys_errno_);

#define SET_REQ_RESULT(req, result_value)                                   \
  req->result = (result_value);                                             \
  if (req->result == -1) {                                                  \
    req->sys_errno_ = _doserrno;                                            \
    req->errorno = uv_translate_sys_error(req->sys_errno_);                 \
  }

#define SET_REQ_WIN32_ERROR(req, sys_errno)                                 \
  req->result = -1;                                                         \
  req->sys_errno_ = (sys_errno);                                            \
  req->errorno = uv_translate_sys_error(req->sys_errno_);

#define SET_REQ_UV_ERROR(req, uv_errno, sys_errno)                          \
  req->result = -1;                                                         \
  req->sys_errno_ = (sys_errno);                                            \
  req->errorno = (uv_errno);

#define VERIFY_UV_FILE(file, req)                                           \
  if (file == -1) {                                                         \
    req->result = -1;                                                       \
    req->errorno = UV_EBADF;                                                \
    req->sys_errno_ = ERROR_INVALID_HANDLE;                                 \
    return;                                                                 \
  }

#define FILETIME_TO_TIME_T(filetime)                                        \
   ((*((uint64_t*) &(filetime)) - 116444736000000000ULL) / 10000000ULL);

#define TIME_T_TO_FILETIME(time, filetime_ptr)                              \
  do {                                                                      \
    *(uint64_t*) (filetime_ptr) = ((int64_t) (time) * 10000000LL) +         \
                                  116444736000000000ULL;                    \
  } while(0)


#define IS_SLASH(c) ((c) == L'\\' || (c) == L'/')
#define IS_LETTER(c) (((c) >= L'a' && (c) <= L'z') || \
  ((c) >= L'A' && (c) <= L'Z'))

const wchar_t JUNCTION_PREFIX[] = L"\\??\\";
const wchar_t JUNCTION_PREFIX_LEN = 4;

const wchar_t LONG_PATH_PREFIX[] = L"\\\\?\\";
const wchar_t LONG_PATH_PREFIX_LEN = 4;


void uv_fs_init() {
  _fmode = _O_BINARY;
}


static void uv_fs_req_init_async(uv_loop_t* loop, uv_fs_t* req,
    uv_fs_type fs_type, const char* path, const wchar_t* pathw, uv_fs_cb cb) {
  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_FS;
  req->loop = loop;
  req->flags = 0;
  req->fs_type = fs_type;
  req->cb = cb;
  req->result = 0;
  req->ptr = NULL;
  req->path = path ? strdup(path) : NULL;
  req->pathw = (wchar_t*)pathw;
  req->errorno = 0;
  req->sys_errno_ = 0;
  memset(&req->overlapped, 0, sizeof(req->overlapped));
}


static void uv_fs_req_init_sync(uv_loop_t* loop, uv_fs_t* req,
    uv_fs_type fs_type) {
  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_FS;
  req->loop = loop;
  req->flags = 0;
  req->fs_type = fs_type;
  req->result = 0;
  req->ptr = NULL;
  req->path = NULL;
  req->pathw = NULL;
  req->errorno = 0;
}


static int is_path_dir(const wchar_t* path) {
  DWORD attr = GetFileAttributesW(path);

  if (attr != INVALID_FILE_ATTRIBUTES) {
    return attr & FILE_ATTRIBUTE_DIRECTORY ? 1 : 0;
  } else {
    return 0;
  }
}


INLINE static int fs__readlink_handle(HANDLE handle, char** target_ptr,
    int64_t* target_len_ptr) {
  char buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
  REPARSE_DATA_BUFFER* reparse_data = (REPARSE_DATA_BUFFER*) buffer;
  WCHAR *w_target;
  DWORD w_target_len;
  char* target;
  int target_len;
  DWORD bytes;

  if (!DeviceIoControl(handle,
                       FSCTL_GET_REPARSE_POINT,
                       NULL,
                       0,
                       buffer,
                       sizeof buffer,
                       &bytes,
                       NULL)) {
    return -1;
  }

  if (reparse_data->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
    /* Real symlink */
    w_target = reparse_data->SymbolicLinkReparseBuffer.PathBuffer +
        (reparse_data->SymbolicLinkReparseBuffer.SubstituteNameOffset /
        sizeof(WCHAR));
    w_target_len =
        reparse_data->SymbolicLinkReparseBuffer.SubstituteNameLength /
        sizeof(WCHAR);

    /* Real symlinks can contain pretty much everything, but the only thing */
    /* we really care about is undoing the implicit conversion to an NT */
    /* namespaced path that CreateSymbolicLink will perform on absolute */
    /* paths. If the path is win32-namespaced then the user must have */
    /* explicitly made it so, and we better just return the unmodified */
    /* reparse data. */
    if (w_target_len >= 4 &&
        w_target[0] == L'\\' &&
        w_target[1] == L'?' &&
        w_target[2] == L'?' &&
        w_target[3] == L'\\') {
      /* Starts with \??\ */
      if (w_target_len >= 6 &&
          ((w_target[4] >= L'A' && w_target[4] <= L'Z') ||
           (w_target[4] >= L'a' && w_target[4] <= L'z')) &&
          w_target[5] == L':' &&
          (w_target_len == 6 || w_target[6] == L'\\')) {
        /* \??\�drive�:\ */
        w_target += 4;
        w_target_len -= 4;

      } else if (w_target_len >= 8 &&
                 (w_target[4] == L'U' || w_target[4] == L'u') &&
                 (w_target[5] == L'N' || w_target[5] == L'n') &&
                 (w_target[6] == L'C' || w_target[6] == L'c') &&
                 w_target[7] == L'\\') {
        /* \??\UNC\�server�\�share�\ - make sure the final path looks like */
        /* \\�server�\�share�\ */
        w_target += 6;
        w_target[0] = L'\\';
        w_target_len -= 6;
      }
    }

  } else if (reparse_data->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
    /* Junction. */
    w_target = reparse_data->MountPointReparseBuffer.PathBuffer +
        (reparse_data->MountPointReparseBuffer.SubstituteNameOffset /
        sizeof(WCHAR));
    w_target_len = reparse_data->MountPointReparseBuffer.SubstituteNameLength /
        sizeof(wchar_t);

    /* Only treat junctions that look like \??\�drive�:\ as symlink. */
    /* Junctions can also be used as mount points, like \??\Volume{�guid�}, */
    /* but that's confusing for programs since they wouldn't be able to */
    /* actually understand such a path when returned by uv_readlink(). */
    /* UNC paths are never valid for junctions so we don't care about them. */
    if (!(w_target_len >= 6 &&
          w_target[0] == L'\\' &&
          w_target[1] == L'?' &&
          w_target[2] == L'?' &&
          w_target[3] == L'\\' &&
          ((w_target[4] >= L'A' && w_target[4] <= L'Z') ||
           (w_target[4] >= L'a' && w_target[4] <= L'z')) &&
          w_target[5] == L':' &&
          (w_target_len == 6 || w_target[6] == L'\\'))) {
      SetLastError(ERROR_SYMLINK_NOT_SUPPORTED);
      return -1;
    }

    /* Remove leading \??\ */
    w_target += 4;
    w_target_len -= 4;

  } else {
    /* Reparse tag does not indicate a symlink. */
    SetLastError(ERROR_SYMLINK_NOT_SUPPORTED);
    return -1;
  }

  /* Compute the length of the target. */
  target_len = WideCharToMultiByte(CP_UTF8,
                                   0,
                                   w_target,
                                   w_target_len,
                                   NULL,
                                   0,
                                   NULL,
                                   NULL);
  if (target_len == 0) {
    return -1;
  }

  /* If requested, allocate memory and convert to UTF8. */
  if (target_ptr != NULL) {
    int r;
    target = (char*) malloc(target_len + 1);
    if (target == NULL) {
      SetLastError(ERROR_OUTOFMEMORY);
      return -1;
    }

    r = WideCharToMultiByte(CP_UTF8,
                            0,
                            w_target,
                            w_target_len,
                            target,
                            target_len,
                            NULL,
                            NULL);
    assert(r == target_len);
    target[target_len] = '\0';

    *target_ptr = target;
  }

  if (target_len_ptr != NULL) {
    *target_len_ptr = target_len;
  }

  return 0;
}


void fs__open(uv_fs_t* req, const wchar_t* path, int flags, int mode) {
  DWORD access;
  DWORD share;
  DWORD disposition;
  DWORD attributes;
  HANDLE file;
  int result, current_umask;

  /* Obtain the active umask. umask() never fails and returns the previous */
  /* umask. */
  current_umask = umask(0);
  umask(current_umask);

  /* convert flags and mode to CreateFile parameters */
  switch (flags & (_O_RDONLY | _O_WRONLY | _O_RDWR)) {
  case _O_RDONLY:
    access = FILE_GENERIC_READ;
    break;
  case _O_WRONLY:
    access = FILE_GENERIC_WRITE;
    break;
  case _O_RDWR:
    access = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
    break;
  default:
    result  = -1;
    goto end;
  }

  if (flags & _O_APPEND) {
    access &= ~FILE_WRITE_DATA;
    access |= FILE_APPEND_DATA;
  }

  /*
   * Here is where we deviate significantly from what CRT's _open()
   * does. We indiscriminately use all the sharing modes, to match
   * UNIX semantics. In particular, this ensures that the file can
   * be deleted even whilst it's open, fixing issue #1449.
   */
  share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

  switch (flags & (_O_CREAT | _O_EXCL | _O_TRUNC)) {
  case 0:
  case _O_EXCL:
    disposition = OPEN_EXISTING;
    break;
  case _O_CREAT:
    disposition = OPEN_ALWAYS;
    break;
  case _O_CREAT | _O_EXCL:
  case _O_CREAT | _O_TRUNC | _O_EXCL:
    disposition = CREATE_NEW;
    break;
  case _O_TRUNC:
  case _O_TRUNC | _O_EXCL:
    disposition = TRUNCATE_EXISTING;
    break;
  case _O_CREAT | _O_TRUNC:
    disposition = CREATE_ALWAYS;
    break;
  default:
    result = -1;
    goto end;
  }

  attributes = FILE_ATTRIBUTE_NORMAL;
  if (flags & _O_CREAT) {
    if (!((mode & ~current_umask) & _S_IWRITE)) {
      attributes |= FILE_ATTRIBUTE_READONLY;
    }
  }

  if (flags & _O_TEMPORARY ) {
    attributes |= FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY;
    access |= DELETE;
  }

  if (flags & _O_SHORT_LIVED) {
    attributes |= FILE_ATTRIBUTE_TEMPORARY;
  }

  switch (flags & (_O_SEQUENTIAL | _O_RANDOM)) {
  case 0:
    break;
  case _O_SEQUENTIAL:
    attributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    break;
  case _O_RANDOM:
    attributes |= FILE_FLAG_RANDOM_ACCESS;
    break;
  default:
    result = -1;
    goto end;
  }

  /* Setting this flag makes it possible to open a directory. */
  attributes |= FILE_FLAG_BACKUP_SEMANTICS;

  file = CreateFileW(path,
                     access,
                     share,
                     NULL,
                     disposition,
                     attributes,
                     NULL);
  if (file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_EXISTS && (flags & _O_CREAT) &&
        !(flags & _O_EXCL)) {
      /* Special case: when ERROR_FILE_EXISTS happens and O_CREAT was */
      /* specified, it means the path referred to a directory. */
      SET_REQ_UV_ERROR(req, UV_EISDIR, error);
    } else {
      SET_REQ_WIN32_ERROR(req, GetLastError());
    }
    return;
  }
  result = _open_osfhandle((intptr_t)file, flags);
end:
  SET_REQ_RESULT(req, result);
}

void fs__close(uv_fs_t* req, uv_file file) {
  int result;

  VERIFY_UV_FILE(file, req);

  result = _close(file);
  SET_REQ_RESULT(req, result);
}


void fs__read(uv_fs_t* req, uv_file file, void *buf, size_t length,
    int64_t offset) {
  HANDLE handle;
  OVERLAPPED overlapped, *overlapped_ptr;
  LARGE_INTEGER offset_;
  DWORD bytes;
  DWORD error;

  VERIFY_UV_FILE(file, req);

  handle = (HANDLE) _get_osfhandle(file);
  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_RESULT(req, -1);
    return;
  }

  if (length > INT_MAX) {
    SET_REQ_WIN32_ERROR(req, ERROR_INSUFFICIENT_BUFFER);
    return;
  }

  if (offset != -1) {
    memset(&overlapped, 0, sizeof overlapped);

    offset_.QuadPart = offset;
    overlapped.Offset = offset_.LowPart;
    overlapped.OffsetHigh = offset_.HighPart;

    overlapped_ptr = &overlapped;
  } else {
    overlapped_ptr = NULL;
  }

  if (ReadFile(handle, buf, length, &bytes, overlapped_ptr)) {
    SET_REQ_RESULT(req, bytes);
  } else {
    error = GetLastError();
    if (error == ERROR_HANDLE_EOF) {
      SET_REQ_RESULT(req, bytes);
    } else {
      SET_REQ_WIN32_ERROR(req, error);
    }
  }
}


void fs__write(uv_fs_t* req, uv_file file, void *buf, size_t length,
    int64_t offset) {
  HANDLE handle;
  OVERLAPPED overlapped, *overlapped_ptr;
  LARGE_INTEGER offset_;
  DWORD bytes;

  VERIFY_UV_FILE(file, req);

  handle = (HANDLE) _get_osfhandle(file);
  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_RESULT(req, -1);
    return;
  }

  if (length > INT_MAX) {
    SET_REQ_WIN32_ERROR(req, ERROR_INSUFFICIENT_BUFFER);
    return;
  }

  if (offset != -1) {
    memset(&overlapped, 0, sizeof overlapped);

    offset_.QuadPart = offset;
    overlapped.Offset = offset_.LowPart;
    overlapped.OffsetHigh = offset_.HighPart;

    overlapped_ptr = &overlapped;
  } else {
    overlapped_ptr = NULL;
  }

  if (WriteFile(handle, buf, length, &bytes, overlapped_ptr)) {
    SET_REQ_RESULT(req, bytes);
  } else {
    SET_REQ_WIN32_ERROR(req, GetLastError());
  }
}


void fs__rmdir(uv_fs_t* req, const wchar_t* path) {
  int result = _wrmdir(path);
  SET_REQ_RESULT(req, result);
}


void fs__unlink(uv_fs_t* req, const wchar_t* path) {
  int result;
  HANDLE handle;
  BY_HANDLE_FILE_INFORMATION info;
  int is_dir_symlink;

  handle = CreateFileW(path,
                       0,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                       NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  if (!GetFileInformationByHandle(handle, &info)) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    CloseHandle(handle);
    return;
  }

  is_dir_symlink = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                   (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);

  CloseHandle(handle);

  /* Todo: very inefficient; fix this. */
  if (is_dir_symlink) {
    fs__rmdir(req, path);
  } else {
    result = _wunlink(path);
    SET_REQ_RESULT(req, result);
  }
}


void fs__mkdir(uv_fs_t* req, const wchar_t* path, int mode) {
  int result = _wmkdir(path);
  SET_REQ_RESULT(req, result);
}


void fs__readdir(uv_fs_t* req, const wchar_t* path, int flags) {
  int result, size;
  wchar_t* buf = NULL, *ptr, *name;
  HANDLE dir;
  WIN32_FIND_DATAW ent = {0};
  size_t len = wcslen(path);
  size_t buf_char_len = 4096;
  wchar_t* path2;
  const wchar_t* fmt = !len                                         ? L"./*"
                : (path[len - 1] == L'/' || path[len - 1] == L'\\') ? L"%s*"
                :                                                     L"%s\\*";

  /* Figure out whether path is a file or a directory. */
  if (!(GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY)) {
    req->result = -1;
    req->errorno = UV_ENOTDIR;
    req->sys_errno_ = ERROR_SUCCESS;
    return;
  }

  path2 = (wchar_t*)malloc(sizeof(wchar_t) * (len + 4));
  if (!path2) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

#ifdef _MSC_VER
  swprintf(path2, len + 3, fmt, path);
#else
  swprintf(path2, fmt, path);
#endif
  dir = FindFirstFileW(path2, &ent);
  free(path2);

  if(dir == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  result = 0;

  do {
    name = ent.cFileName;

    if (name[0] != L'.' || (name[1] && (name[1] != L'.' || name[2]))) {
      len = wcslen(name);

      if (!buf) {
        buf = (wchar_t*)malloc(buf_char_len * sizeof(wchar_t));
        if (!buf) {
          uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
        }

        ptr = buf;
      }

      while ((ptr - buf) + len + 1 > buf_char_len) {
        buf_char_len *= 2;
        path2 = buf;
        buf = (wchar_t*)realloc(buf, buf_char_len * sizeof(wchar_t));
        if (!buf) {
          uv_fatal_error(ERROR_OUTOFMEMORY, "realloc");
        }

        ptr = buf + (ptr - path2);
      }

      wcscpy(ptr, name);
      ptr += len + 1;
      result++;
    }
  } while(FindNextFileW(dir, &ent));

  FindClose(dir);

  if (buf) {
    /* Convert result to UTF8. */
    size = uv_utf16_to_utf8(buf, buf_char_len, NULL, 0);
    if (!size) {
      SET_REQ_WIN32_ERROR(req, GetLastError());
      return;
    }

    req->ptr = (char*)malloc(size + 1);
    if (!req->ptr) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    size = uv_utf16_to_utf8(buf, buf_char_len, (char*)req->ptr, size);
    if (!size) {
      free(buf);
      free(req->ptr);
      req->ptr = NULL;
      SET_REQ_WIN32_ERROR(req, GetLastError());
      return;
    }
    free(buf);

    ((char*)req->ptr)[size] = '\0';
    req->flags |= UV_FS_FREE_PTR;
  } else {
    req->ptr = NULL;
  }

  SET_REQ_RESULT(req, result);
}


INLINE static int fs__stat_handle(HANDLE handle, uv_statbuf_t* statbuf) {
  BY_HANDLE_FILE_INFORMATION info;

  if (!GetFileInformationByHandle(handle, &info)) {
    return -1;
  }

  /* TODO: set st_dev, st_rdev and st_ino to something meaningful. */
  statbuf->st_ino = 0;
  statbuf->st_dev = 0;
  statbuf->st_rdev = 0;

  statbuf->st_gid = 0;
  statbuf->st_uid = 0;

  statbuf->st_mode = 0;

  if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    if (fs__readlink_handle(handle, NULL, &statbuf->st_size) != 0) {
      return -1;
    }
    statbuf->st_mode |= S_IFLNK;
  } else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    statbuf->st_mode |= _S_IFDIR;
    statbuf->st_size = 0;
  } else {
    statbuf->st_mode |= _S_IFREG;
    statbuf->st_size = ((int64_t) info.nFileSizeHigh << 32) +
                        (int64_t) info.nFileSizeLow;
  }

  if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
    statbuf->st_mode |= (_S_IREAD + (_S_IREAD >> 3) + (_S_IREAD >> 6));
  } else {
    statbuf->st_mode |= ((_S_IREAD|_S_IWRITE) + ((_S_IREAD|_S_IWRITE) >> 3) +
      ((_S_IREAD|_S_IWRITE) >> 6));
  }

  statbuf->st_mtime = FILETIME_TO_TIME_T(info.ftLastWriteTime);
  statbuf->st_atime = FILETIME_TO_TIME_T(info.ftLastAccessTime);
  statbuf->st_ctime = FILETIME_TO_TIME_T(info.ftCreationTime);

  statbuf->st_nlink = (info.nNumberOfLinks <= SHRT_MAX) ?
                      (short) info.nNumberOfLinks : SHRT_MAX;

  return 0;
}


INLINE static void fs__stat(uv_fs_t* req, const wchar_t* path, int do_lstat) {
  HANDLE handle;
  DWORD flags;

  req->ptr = NULL;
  flags = FILE_FLAG_BACKUP_SEMANTICS;

  if (do_lstat) {
    flags |= FILE_FLAG_OPEN_REPARSE_POINT;
  }

  handle = CreateFileW(path,
                       FILE_READ_ATTRIBUTES,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL,
                       OPEN_EXISTING,
                       flags,
                       NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  if (fs__stat_handle(handle, &req->stat) != 0) {
    DWORD error = GetLastError();
    if (do_lstat && error == ERROR_SYMLINK_NOT_SUPPORTED) {
      /* We opened a reparse point but it was not a symlink. Try again. */
      fs__stat(req, path, 0);

    } else {
      /* Stat failed. */
      SET_REQ_WIN32_ERROR(req, GetLastError());
    }

    CloseHandle(handle);
    return;
  }

  req->ptr = &req->stat;
  req->result = 0;
  CloseHandle(handle);
}


void fs__fstat(uv_fs_t* req, uv_file file) {
  HANDLE handle;

  req->ptr = NULL;

  VERIFY_UV_FILE(file, req);

  handle = (HANDLE) _get_osfhandle(file);

  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, ERROR_INVALID_HANDLE);
    return;
  }

  if (fs__stat_handle(handle, &req->stat) != 0) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  req->ptr = &req->stat;
  req->result = 0;
}


void fs__rename(uv_fs_t* req, const wchar_t* path, const wchar_t* new_path) {
  if (!MoveFileExW(path, new_path, MOVEFILE_REPLACE_EXISTING)) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  SET_REQ_RESULT(req, 0);
}


void fs__fsync(uv_fs_t* req, uv_file file) {
  int result;

  VERIFY_UV_FILE(file, req);

  result = FlushFileBuffers((HANDLE)_get_osfhandle(file)) ? 0 : -1;
  if (result == -1) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
  } else {
    SET_REQ_RESULT(req, result);
  }
}


void fs__ftruncate(uv_fs_t* req, uv_file file, int64_t offset) {
  HANDLE handle;
  NTSTATUS status;
  IO_STATUS_BLOCK io_status;
  FILE_END_OF_FILE_INFORMATION eof_info;

  VERIFY_UV_FILE(file, req);

  handle = (HANDLE)_get_osfhandle(file);

  eof_info.EndOfFile.QuadPart = offset;

  status = pNtSetInformationFile(handle,
                                 &io_status,
                                 &eof_info,
                                 sizeof eof_info,
                                 FileEndOfFileInformation);

  if (NT_SUCCESS(status)) {
    SET_REQ_RESULT(req, 0);
  } else {
    SET_REQ_WIN32_ERROR(req, pRtlNtStatusToDosError(status));
  }
}


void fs__sendfile(uv_fs_t* req, uv_file out_file, uv_file in_file,
    int64_t in_offset, size_t length) {
  const size_t max_buf_size = 65536;
  size_t buf_size = length < max_buf_size ? length : max_buf_size;
  int n, result = 0;
  int64_t result_offset = 0;
  char* buf = (char*)malloc(buf_size);
  if (!buf) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (in_offset != -1) {
    result_offset = _lseeki64(in_file, in_offset, SEEK_SET);
  }

  if (result_offset == -1) {
    result = -1;
  } else {
    while (length > 0) {
      n = _read(in_file, buf, length < buf_size ? length : buf_size);
      if (n == 0) {
        break;
      } else if (n == -1) {
        result = -1;
        break;
      }

      length -= n;

      n = _write(out_file, buf, n);
      if (n == -1) {
        result = -1;
        break;
      }

      result += n;
    }
  }

  SET_REQ_RESULT(req, result);
}


void fs__chmod(uv_fs_t* req, const wchar_t* path, int mode) {
  int result = _wchmod(path, mode);
  SET_REQ_RESULT(req, result);
}


void fs__fchmod(uv_fs_t* req, uv_file file, int mode) {
  int result;
  HANDLE handle;
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_BASIC_INFORMATION file_info;

  VERIFY_UV_FILE(file, req);

  handle = (HANDLE)_get_osfhandle(file);

  nt_status = pNtQueryInformationFile(handle,
                                      &io_status,
                                      &file_info,
                                      sizeof file_info,
                                      FileBasicInformation);

  if (nt_status != STATUS_SUCCESS) {
    result = -1;
    goto done;
  }

  if (mode & _S_IWRITE) {
    file_info.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
  } else {
    file_info.FileAttributes |= FILE_ATTRIBUTE_READONLY;
  }

  nt_status = pNtSetInformationFile(handle,
                                    &io_status,
                                    &file_info,
                                    sizeof file_info,
                                    FileBasicInformation);

  if (nt_status != STATUS_SUCCESS) {
    result = -1;
    goto done;
  }

  result = 0;

done:
  SET_REQ_RESULT(req, result);
}


INLINE static int fs__utime_handle(HANDLE handle, double atime, double mtime) {
  FILETIME filetime_a, filetime_m;

  TIME_T_TO_FILETIME((time_t) atime, &filetime_a);
  TIME_T_TO_FILETIME((time_t) mtime, &filetime_m);

  if (!SetFileTime(handle, NULL, &filetime_a, &filetime_m)) {
    return -1;
  }

  return 0;
}


void fs__utime(uv_fs_t* req, const wchar_t* path, double atime, double mtime) {
  HANDLE handle;

  handle = CreateFileW(path,
                       FILE_WRITE_ATTRIBUTES,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS,
                       NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  if (fs__utime_handle(handle, atime, mtime) != 0) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  req->result = 0;
}


void fs__futime(uv_fs_t* req, uv_file file, double atime, double mtime) {
  HANDLE handle;
  VERIFY_UV_FILE(file, req);

  handle = (HANDLE) _get_osfhandle(file);

  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, ERROR_INVALID_HANDLE);
    return;
  }

  if (fs__utime_handle(handle, atime, mtime) != 0) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  req->result = 0;
}


void fs__link(uv_fs_t* req, const wchar_t* path, const wchar_t* new_path) {
  int result = CreateHardLinkW(new_path, path, NULL) ? 0 : -1;
  if (result == -1) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
  } else {
    SET_REQ_RESULT(req, result);
  }
}


void fs__create_junction(uv_fs_t* req, const wchar_t* path, const wchar_t* new_path) {
  HANDLE handle = INVALID_HANDLE_VALUE;
  REPARSE_DATA_BUFFER *buffer = NULL;
  int created = 0;
  int target_len;
  int is_absolute, is_long_path;
  int needed_buf_size, used_buf_size, used_data_size, path_buf_len;
  int start, len, i;
  int add_slash;
  DWORD bytes;
  wchar_t* path_buf;

  target_len = wcslen(path);
  is_long_path = wcsncmp(path, LONG_PATH_PREFIX, LONG_PATH_PREFIX_LEN) == 0;

  if (is_long_path) {
    is_absolute = 1;
  } else {
    is_absolute = target_len >= 3 && IS_LETTER(path[0]) &&
      path[1] == L':' && IS_SLASH(path[2]);
  }

  if (!is_absolute) {
    /* Not supporting relative paths */
    SET_REQ_UV_ERROR(req, UV_EINVAL, ERROR_NOT_SUPPORTED);
    return;
  }

  // Do a pessimistic calculation of the required buffer size
  needed_buf_size =
      FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) +
      JUNCTION_PREFIX_LEN * sizeof(wchar_t) +
      2 * (target_len + 2) * sizeof(wchar_t);

  // Allocate the buffer
  buffer = (REPARSE_DATA_BUFFER*)malloc(needed_buf_size);
  if (!buffer) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  // Grab a pointer to the part of the buffer where filenames go
  path_buf = (wchar_t*)&(buffer->MountPointReparseBuffer.PathBuffer);
  path_buf_len = 0;

  // Copy the substitute (internal) target path
  start = path_buf_len;

  wcsncpy((wchar_t*)&path_buf[path_buf_len], JUNCTION_PREFIX,
    JUNCTION_PREFIX_LEN);
  path_buf_len += JUNCTION_PREFIX_LEN;

  add_slash = 0;
  for (i = is_long_path ? LONG_PATH_PREFIX_LEN : 0; path[i] != L'\0'; i++) {
    if (IS_SLASH(path[i])) {
      add_slash = 1;
      continue;
    }

    if (add_slash) {
      path_buf[path_buf_len++] = L'\\';
      add_slash = 0;
    }

    path_buf[path_buf_len++] = path[i];
  }
  path_buf[path_buf_len++] = L'\\';
  len = path_buf_len - start;

  // Set the info about the substitute name
  buffer->MountPointReparseBuffer.SubstituteNameOffset = start * sizeof(wchar_t);
  buffer->MountPointReparseBuffer.SubstituteNameLength = len * sizeof(wchar_t);

  // Insert null terminator
  path_buf[path_buf_len++] = L'\0';

  // Copy the print name of the target path
  start = path_buf_len;
  add_slash = 0;
  for (i = is_long_path ? LONG_PATH_PREFIX_LEN : 0; path[i] != L'\0'; i++) {
    if (IS_SLASH(path[i])) {
      add_slash = 1;
      continue;
    }

    if (add_slash) {
      path_buf[path_buf_len++] = L'\\';
      add_slash = 0;
    }

    path_buf[path_buf_len++] = path[i];
  }
  len = path_buf_len - start;
  if (len == 2) {
    path_buf[path_buf_len++] = L'\\';
    len++;
  }

  // Set the info about the print name
  buffer->MountPointReparseBuffer.PrintNameOffset = start * sizeof(wchar_t);
  buffer->MountPointReparseBuffer.PrintNameLength = len * sizeof(wchar_t);

  // Insert another null terminator
  path_buf[path_buf_len++] = L'\0';

  // Calculate how much buffer space was actually used
  used_buf_size = FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) +
    path_buf_len * sizeof(wchar_t);
  used_data_size = used_buf_size -
    FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer);

  // Put general info in the data buffer
  buffer->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  buffer->ReparseDataLength = used_data_size;
  buffer->Reserved = 0;

  // Create a new directory
  if (!CreateDirectoryW(new_path, NULL)) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    goto error;
  }
  created = 1;

  // Open the directory
  handle = CreateFileW(new_path,
                       GENERIC_ALL,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS |
                         FILE_FLAG_OPEN_REPARSE_POINT,
                       NULL);
  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    goto error;
  }

  // Create the actual reparse point
  if (!DeviceIoControl(handle,
                       FSCTL_SET_REPARSE_POINT,
                       buffer,
                       used_buf_size,
                       NULL,
                       0,
                       &bytes,
                       NULL)) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    goto error;
  }

  // Clean up
  CloseHandle(handle);
  free(buffer);

  SET_REQ_RESULT(req, 0);
  return;

error:
  free(buffer);

  if (handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle);
  }

  if (created) {
    RemoveDirectoryW(new_path);
  }
}


void fs__symlink(uv_fs_t* req, const wchar_t* path, const wchar_t* new_path,
                 int flags) {
  int result;

  if (flags & UV_FS_SYMLINK_JUNCTION) {
    fs__create_junction(req, path, new_path);
  } else if (pCreateSymbolicLinkW) {
    result = pCreateSymbolicLinkW(new_path,
                                  path,
                                  flags & UV_FS_SYMLINK_DIR ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) ? 0 : -1;
    if (result == -1) {
      SET_REQ_WIN32_ERROR(req, GetLastError());
    } else {
      SET_REQ_RESULT(req, result);
    }
  } else {
    SET_REQ_UV_ERROR(req, UV_ENOSYS, ERROR_NOT_SUPPORTED);
  }
}


void fs__readlink(uv_fs_t* req, const wchar_t* path) {
  HANDLE handle;

  handle = CreateFileW(path,
                       0,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                       NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    return;
  }

  if (fs__readlink_handle(handle, (char**) &req->ptr, NULL) != 0) {
    SET_REQ_WIN32_ERROR(req, GetLastError());
    CloseHandle(handle);
    return;
  }

  req->flags |= UV_FS_FREE_PTR;
  SET_REQ_RESULT(req, 0);

  CloseHandle(handle);
}


void fs__nop(uv_fs_t* req) {
  req->result = 0;
}


static DWORD WINAPI uv_fs_thread_proc(void* parameter) {
  uv_fs_t* req = (uv_fs_t*) parameter;
  uv_loop_t* loop = req->loop;

  assert(req != NULL);
  assert(req->type == UV_FS);

  switch (req->fs_type) {
    case UV_FS_OPEN:
      fs__open(req, req->pathw, req->file_flags, (int)req->mode);
      break;
    case UV_FS_CLOSE:
      fs__close(req, req->file);
      break;
    case UV_FS_READ:
      fs__read(req, req->file, req->buf, req->length, req->offset);
      break;
    case UV_FS_WRITE:
      fs__write(req, req->file, req->buf, req->length, req->offset);
      break;
    case UV_FS_UNLINK:
      fs__unlink(req, req->pathw);
      break;
    case UV_FS_MKDIR:
      fs__mkdir(req, req->pathw, req->mode);
      break;
    case UV_FS_RMDIR:
      fs__rmdir(req, req->pathw);
      break;
    case UV_FS_READDIR:
      fs__readdir(req, req->pathw, req->file_flags);
      break;
    case UV_FS_STAT:
      fs__stat(req, req->pathw, 0);
      break;
    case UV_FS_LSTAT:
      fs__stat(req, req->pathw, 1);
      break;
    case UV_FS_FSTAT:
      fs__fstat(req, req->file);
      break;
    case UV_FS_RENAME:
      fs__rename(req, req->pathw, req->new_pathw);
      break;
    case UV_FS_FSYNC:
    case UV_FS_FDATASYNC:
      fs__fsync(req, req->file);
      break;
    case UV_FS_FTRUNCATE:
      fs__ftruncate(req, req->file, req->offset);
      break;
    case UV_FS_SENDFILE:
      fs__sendfile(req, req->file_out, req->file, req->offset, req->length);
      break;
    case UV_FS_CHMOD:
      fs__chmod(req, req->pathw, req->mode);
      break;
    case UV_FS_FCHMOD:
      fs__fchmod(req, req->file, req->mode);
      break;
    case UV_FS_UTIME:
      fs__utime(req, req->pathw, req->atime, req->mtime);
      break;
    case UV_FS_FUTIME:
      fs__futime(req, req->file, req->atime, req->mtime);
      break;
    case UV_FS_LINK:
      fs__link(req, req->pathw, req->new_pathw);
      break;
    case UV_FS_SYMLINK:
      fs__symlink(req, req->pathw, req->new_pathw, req->file_flags);
      break;
    case UV_FS_READLINK:
      fs__readlink(req, req->pathw);
      break;
    case UV_FS_CHOWN:
    case UV_FS_FCHOWN:
      fs__nop(req);
      break;
    default:
      assert(!"bad uv_fs_type");
  }

  POST_COMPLETION_FOR_REQ(loop, req);

  return 0;
}


int uv_fs_open(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    int mode, uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_OPEN, path, pathw, cb);
    req->file_flags = flags;
    req->mode = mode;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_OPEN);
    fs__open(req, pathw, flags, mode);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_CLOSE, NULL, NULL, cb);
    req->file = file;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_CLOSE);
    fs__close(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, int64_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_READ, NULL, NULL, cb);
    req->file = file;
    req->buf = buf;
    req->length = length;
    req->offset = offset;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_READ);
    fs__read(req, file, buf, length, offset);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, int64_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_WRITE, NULL, NULL, cb);
    req->file = file;
    req->buf = buf;
    req->length = length;
    req->offset = offset;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_WRITE);
    fs__write(req, file, buf, length, offset);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_UNLINK, path, pathw, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_UNLINK);
    fs__unlink(req, pathw);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_mkdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_MKDIR, path, pathw, cb);
    req->mode = mode;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_MKDIR);
    fs__mkdir(req, pathw, mode);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_RMDIR, path, pathw, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_RMDIR);
    fs__rmdir(req, pathw);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_readdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_READDIR, path, pathw, cb);
    req->file_flags = flags;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_READDIR);
    fs__readdir(req, pathw, flags);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_link(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  wchar_t* pathw;
  wchar_t* new_pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);
  UTF8_TO_UTF16(new_path, new_pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_LINK, path, pathw, cb);
    req->new_pathw = new_pathw;
    req->flags |= UV_FS_FREE_NEW_PATH;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_LINK);
    fs__link(req, pathw, new_pathw);
    free(pathw);
    free(new_pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_symlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, int flags, uv_fs_cb cb) {
  wchar_t* pathw;
  wchar_t* new_pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);
  UTF8_TO_UTF16(new_path, new_pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_SYMLINK, path, pathw, cb);
    req->new_pathw = new_pathw;
    req->file_flags = flags;
    req->flags |= UV_FS_FREE_NEW_PATH;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_SYMLINK);
    fs__symlink(req, pathw, new_pathw, flags);
    free(pathw);
    free(new_pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_readlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_READLINK, path, pathw, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_READLINK);
    fs__readlink(req, pathw);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_chown(uv_loop_t* loop, uv_fs_t* req, const char* path, int uid,
    int gid, uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_CHOWN, path, pathw, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_CHOWN);
    fs__nop(req);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file, int uid,
    int gid, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FCHOWN, NULL, NULL, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FCHOWN);
    fs__nop(req);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  int len = strlen(path);
  char* path2 = NULL;
  wchar_t* pathw;
  int size;

  if (len > 1 && path[len - 2] != ':' &&
      (path[len - 1] == '\\' || path[len - 1] == '/')) {
    path2 = strdup(path);
    if (!path2) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    path2[len - 1] = '\0';
  }

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_STAT, NULL, NULL, cb);
    if (path2) {
      req->path = path2;
      UTF8_TO_UTF16(path2, req->pathw);
    } else {
      req->path = strdup(path);
      UTF8_TO_UTF16(path, req->pathw);
    }

    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_STAT);
    UTF8_TO_UTF16(path2 ? path2 : path, pathw);
    fs__stat(req, pathw, 0);
    if (path2) {
      free(path2);
    }
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


/* TODO: add support for links. */
int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  int len = strlen(path);
  char* path2 = NULL;
  wchar_t* pathw;
  int size;

  if (len > 1 && path[len - 2] != ':' &&
      (path[len - 1] == '\\' || path[len - 1] == '/')) {
    path2 = strdup(path);
    if (!path2) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    path2[len - 1] = '\0';
  }

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_LSTAT, NULL, NULL, cb);
     if (path2) {
      req->path = path2;
      UTF8_TO_UTF16(path2, req->pathw);
    } else {
      req->path = strdup(path);
      UTF8_TO_UTF16(path, req->pathw);
    }

    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_LSTAT);
    UTF8_TO_UTF16(path2 ? path2 : path, pathw);
    fs__stat(req, pathw, 1);
    if (path2) {
      free(path2);
    }
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FSTAT, NULL, NULL, cb);
    req->file = file;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FSTAT);
    fs__fstat(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  wchar_t* pathw;
  wchar_t* new_pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);
  UTF8_TO_UTF16(new_path, new_pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_RENAME, path, pathw, cb);
    req->new_pathw = new_pathw;
    req->flags |= UV_FS_FREE_NEW_PATH;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_RENAME);
    fs__rename(req, pathw, new_pathw);
    free(pathw);
    free(new_pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FDATASYNC, NULL, NULL, cb);
    req->file = file;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FDATASYNC);
    fs__fsync(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FSYNC, NULL, NULL, cb);
    req->file = file;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FSYNC);
    fs__fsync(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    int64_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FTRUNCATE, NULL, NULL, cb);
    req->file = file;
    req->offset = offset;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FTRUNCATE);
    fs__ftruncate(req, file, offset);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_sendfile(uv_loop_t* loop, uv_fs_t* req, uv_file out_fd,
    uv_file in_fd, int64_t in_offset, size_t length, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_SENDFILE, NULL, NULL, cb);
    req->file_out = out_fd;
    req->file = in_fd;
    req->offset = in_offset;
    req->length = length;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_SENDFILE);
    fs__sendfile(req, out_fd, in_fd, in_offset, length);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_chmod(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_CHMOD, path, pathw, cb);
    req->mode = mode;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_CHMOD);
    fs__chmod(req, pathw, mode);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file, int mode,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FCHMOD, NULL, NULL, cb);
    req->file = file;
    req->mode = mode;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FCHMOD);
    fs__fchmod(req, file, mode);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path, double atime,
    double mtime, uv_fs_cb cb) {
  wchar_t* pathw;
  int size;

  /* Convert to UTF16. */
  UTF8_TO_UTF16(path, pathw);

  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_UTIME, path, pathw, cb);
    req->atime = atime;
    req->mtime = mtime;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_UTIME);
    fs__utime(req, pathw, atime, mtime);
    free(pathw);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file, double atime,
    double mtime, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FUTIME, NULL, NULL, cb);
    req->file = file;
    req->atime = atime;
    req->mtime = mtime;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FUTIME);
    fs__futime(req, file, atime, mtime);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


void uv_process_fs_req(uv_loop_t* loop, uv_fs_t* req) {
  assert(req->cb);
  uv__req_unregister(loop, req);
  SET_UV_LAST_ERROR_FROM_REQ(req);
  req->cb(req);
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  uv_loop_t* loop = req->loop;

  if (req->flags & UV_FS_CLEANEDUP) {
    return;
  }

  if (req->flags & UV_FS_FREE_PATH && req->pathw) {
    free(req->pathw);
    req->pathw = NULL;
  }

  if (req->flags & UV_FS_FREE_NEW_PATH && req->new_pathw) {
    free(req->new_pathw);
    req->new_pathw = NULL;
  }

  if (req->flags & UV_FS_FREE_PTR && req->ptr) {
    free(req->ptr);
  }

  req->ptr = NULL;

  if (req->path) {
    free(req->path);
    req->path = NULL;
  }

  req->flags |= UV_FS_CLEANEDUP;
}

