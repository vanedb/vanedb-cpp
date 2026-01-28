// QuiverDB - Copyright (c) 2025 Anton Tsvetkov - MIT License
#pragma once

#include <string>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace quiverdb {
namespace detail {

/// Cross-platform fsync utility. Reopens file by path, flushes to disk, closes.
/// No-op if the file cannot be opened (best-effort durability).
inline void fsync_file(const std::string& path) {
#if defined(_WIN32) || defined(_WIN64)
  HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE) { FlushFileBuffers(hFile); CloseHandle(hFile); }
#elif defined(__unix__) || defined(__APPLE__)
  int fd = open(path.c_str(), O_WRONLY);
  if (fd >= 0) { fsync(fd); close(fd); }
#endif
}

} // namespace detail
} // namespace quiverdb
