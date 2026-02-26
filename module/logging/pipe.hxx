#pragma once
#include <cstdio>
#include <sstream>
#include <string>
#include <thread>
#include <windows.h>

#define LOG_FUNC() debug_logger::printf("%s()", __FUNCTION__)

namespace debug_logger {

inline bool debug_mode = false;
inline HANDLE pipe_handle = INVALID_HANDLE_VALUE;
inline FILE *log_file = nullptr;

inline std::string get_dll_directory() {
  char path[MAX_PATH] = {0};
  HMODULE hm = NULL;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCSTR)&get_dll_directory, &hm);
  GetModuleFileNameA(hm, path, MAX_PATH);
  std::string dir(path);
  size_t pos = dir.find_last_of("\\/");
  if (pos != std::string::npos)
    dir = dir.substr(0, pos);
  return dir;
}

inline void
initialize_pipe(const std::string &pipe_name = "\\\\.\\pipe\\xynor_debug") {
  // Open log file in DLL directory
  if (!log_file) {
    std::string logPath = get_dll_directory() + "\\vicna_debug.log";
    fopen_s(&log_file, logPath.c_str(), "w");
    if (log_file) {
      fprintf(log_file, "[Vicna] Debug log initialized: %s\n", logPath.c_str());
      fflush(log_file);
    }
  }

  std::thread([pipe_name] {
    while (true) {
      pipe_handle = CreateFileA(pipe_name.c_str(), GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);

      if (pipe_handle != INVALID_HANDLE_VALUE) {
        break;
      }

      DWORD error = GetLastError();
      if (error == ERROR_PIPE_BUSY) {
        if (!WaitNamedPipeA(pipe_name.c_str(), 5000)) {
          continue;
        }
      } else {
        break;
      }
    }
  }).detach();
}

inline void printf(const char *fmt, ...) {
  if (!debug_mode)
    return;

  char buffer[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  strcat_s(buffer, "\n");

  // Write to log file (flush immediately so crash won't lose data)
  if (log_file) {
    fputs(buffer, log_file);
    fflush(log_file);
  }

  OutputDebugStringA(buffer);

  // Also write to pipe if connected
  if (pipe_handle != INVALID_HANDLE_VALUE) {
    DWORD bytes_written = 0;
    WriteFile(pipe_handle, buffer, (DWORD)strlen(buffer), &bytes_written,
              nullptr);
  }
}

template <typename... Args> inline void log(Args &&...args) {
  if (!debug_mode)
    return;

  std::ostringstream ss;
  (ss << ... << args);

  std::string msg = ss.str() + "\n";

  if (log_file) {
    fputs(msg.c_str(), log_file);
    fflush(log_file);
  }

  OutputDebugStringA(msg.c_str());

  if (pipe_handle != INVALID_HANDLE_VALUE) {
    DWORD bytes_written = 0;
    WriteFile(pipe_handle, msg.c_str(), (DWORD)msg.size(), &bytes_written,
              nullptr);
  }
}

inline void cleanup() {
  if (pipe_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_handle);
  }
  pipe_handle = INVALID_HANDLE_VALUE;
  if (log_file) {
    fclose(log_file);
    log_file = nullptr;
  }
}

inline void set_debug_mode(bool enabled) { debug_mode = enabled; }

} // namespace debug_logger
