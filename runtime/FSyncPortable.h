#pragma once

#ifndef _WIN32

#include <unistd.h>

#else

#include <io.h>
#include <Windows.h>

inline int fsync(int fd) {
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (h == INVALID_HANDLE_VALUE) {
    return -1;
  }
  if (!FlushFileBuffers(h)) {
    return -1;
  }
  return 0;
}

inline int fdatasync(int fd) {
  return fsync(fd);
}

#endif