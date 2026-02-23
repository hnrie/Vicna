#include <Windows.h>
#include <zstd/zstd.h>
#include <zstd/xxhash.h>

#include <lstate.h>
#include <lgc.h>
#include <vector>
#include <lmem.h>
#include <string>
#include <vector>

#include <wininet.h>
#include <urlmon.h>

#include <internal/globals.hpp> 
#include <internal/utils.hpp> 
#include <internal/env/yield/yield.hpp> 
#include <cpr/cpr.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")

namespace decompile {
    inline std::string decompress_bytecode(const std::string& c) {
        uint8_t h[4]; memcpy(h, c.data(), 4);
        for (int i = 0; i < 4; i++) h[i] = (h[i] ^ "RSB1"[i]) - i * 41;
        std::vector<uint8_t> v(c.begin(), c.end());
        for (size_t i = 0; i < v.size(); i++) v[i] ^= h[i % 4] + i * 41;
        int len; memcpy(&len, v.data() + 4, 4);
        std::string out(len, 0);
        return ZSTD_decompress(out.data(), len, v.data() + 8, v.size() - 8) == len ? out : "";
    }

    inline std::string read_bytecode(uintptr_t addr) {
        uintptr_t str = addr + 0x10;
        size_t len = *(size_t*)(str + 0x10);
        size_t cap = *(size_t*)(str + 0x18);
        uintptr_t data_ptr = (cap > 0x0f) ? *(uintptr_t*)(str + 0x00) : str;
        return std::string(reinterpret_cast<const char*>(data_ptr), len);
    }

    inline std::string HttpPost(const char* host, unsigned short port, const char* path, const char* data, size_t dataLen) {
        HINTERNET hInternet = InternetOpenA("Leafy/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return "";

        HINTERNET hConnect = InternetConnectA(hInternet, host, port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, 0, 0);
        if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return ""; }

        const char* headers = "Content-Type: text/plain\r\n";
        if (!HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers), (LPVOID)data, (DWORD)dataLen)) {
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string result;
        char buffer[4096];
        DWORD bytesRead;
        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            result.append(buffer, bytesRead);
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return result;
    }

    inline std::string GetScriptBytecodeStr(lua_State* L, int idx) {
        if (lua_type(L, idx) != LUA_TUSERDATA) return "";

        uintptr_t script = *(uintptr_t*)lua_touserdata(L, idx);
        if (!script) return "";

        lua_getfield(L, idx, "ClassName");
        const char* name = lua_tostring(L, -1);
        lua_pop(L, 1);

        uintptr_t addr = name && strcmp(name, "ModuleScript") == 0
            ? *(uintptr_t*)(script + Offsets::Scripts::ModuleScriptByteCode)
            : *(uintptr_t*)(script + Offsets::Scripts::LocalScriptByteCode);

        if (!addr) return "";

        std::string raw = read_bytecode(addr);
        return decompress_bytecode(raw);
    }
}