#pragma once
#include <string>
#include <sstream>
#include <thread>
#include <windows.h>

#define LOG_FUNC() debug_logger::printf("%s()", __FUNCTION__)

namespace debug_logger {

    inline bool debug_mode = false;
    inline HANDLE pipe_handle = INVALID_HANDLE_VALUE;

    inline void initialize_pipe(const std::string& pipe_name = "\\\\.\\pipe\\xynor_debug") {
        std::thread([pipe_name] {
            while (true) {
                pipe_handle = CreateFileA(
                    pipe_name.c_str(),
                    GENERIC_WRITE,
                    0,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr
                );

                if (pipe_handle != INVALID_HANDLE_VALUE) {
                    break;
                }

                DWORD error = GetLastError();
                if (error == ERROR_PIPE_BUSY) {
                    if (!WaitNamedPipeA(pipe_name.c_str(), 5000)) {
                        continue;
                    }
                }
                else {
                    break;
                }
            }
        }).detach();
    }

    inline void printf(const char* fmt, ...) {
        if (!debug_mode) return;
        if (pipe_handle == INVALID_HANDLE_VALUE) return;

        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        strcat_s(buffer, "\n");

        DWORD bytes_written = 0;
        WriteFile(
            pipe_handle,
            buffer,
            (DWORD)strlen(buffer),
            &bytes_written,
            nullptr
        );
    }

    template<typename... Args>
    inline void log(Args&&... args) {
        if (!debug_mode) return;
        if (pipe_handle == INVALID_HANDLE_VALUE) return;

        std::ostringstream ss;
        (ss << ... << args);

        std::string msg = ss.str() + "\n";

        DWORD bytes_written = 0;
        WriteFile(
            pipe_handle,
            msg.c_str(),
            (DWORD)msg.size(),
            &bytes_written,
            nullptr
        );
    }

    inline void cleanup() {
        if (pipe_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_handle);
        }
        pipe_handle = INVALID_HANDLE_VALUE;
    }

    inline void set_debug_mode(bool enabled) {
        debug_mode = enabled;
    }

}
