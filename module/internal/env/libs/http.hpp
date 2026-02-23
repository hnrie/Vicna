#pragma once

#include <Windows.h>
#include <winhttp.h>
#include <lstate.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/env/yield/yield.hpp>

namespace Http
{
    enum RequestMethods
    {
        H_GET,
        H_HEAD,
        H_POST,
        H_PUT,
        H_DELETE,
        H_OPTIONS,
        H_PATCH
    };

    inline std::map<std::string, RequestMethods> RequestMethodMap = {
        { "get", H_GET },
        { "head", H_HEAD },
        { "post", H_POST },
        { "put", H_PUT },
        { "delete", H_DELETE },
        { "options", H_OPTIONS },
        { "patch", H_PATCH }
    };

    const std::vector<std::string> BlockedSites = {
        "https://roblox.com/api",
        "https://accountinformation.roblox.com",
        "https://accountsettings.roblox.com",
        "https://twostepverification.roblox.com",
        "https://trades.roblox.com",
        "https://billing.roblox.com",
        "https://economy.roblox.com",
        "https://auth.roblox.com",
        "https://iplogger.org",
        "https://grabify.link"
    };

    namespace Utils
    {
        inline std::string ToLower(std::string str) {
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            return str;
        }

        inline std::wstring StringToWString(const std::string& str) {
            if (str.empty()) return std::wstring();
            int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
            std::wstring result(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
            return result;
        }

        inline std::string WStringToString(const std::wstring& wstr) {
            if (wstr.empty()) return std::string();
            int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
            std::string result(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, NULL, NULL);
            return result;
        }

        inline bool ParseUrl(const std::string& url, std::wstring& scheme, std::wstring& host,
            std::wstring& path, INTERNET_PORT& port) {
            std::wstring wurl = StringToWString(url);

            URL_COMPONENTSW urlComp = {};
            urlComp.dwStructSize = sizeof(urlComp);

            wchar_t schemeBuffer[32];
            wchar_t hostBuffer[256];
            wchar_t pathBuffer[2048];

            urlComp.lpszScheme = schemeBuffer;
            urlComp.dwSchemeLength = sizeof(schemeBuffer) / sizeof(wchar_t);
            urlComp.lpszHostName = hostBuffer;
            urlComp.dwHostNameLength = sizeof(hostBuffer) / sizeof(wchar_t);
            urlComp.lpszUrlPath = pathBuffer;
            urlComp.dwUrlPathLength = sizeof(pathBuffer) / sizeof(wchar_t);

            if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp))
                return false;

            scheme = urlComp.lpszScheme;
            host = urlComp.lpszHostName;
            path = urlComp.lpszUrlPath;
            port = urlComp.nPort;

            return true;
        }
    }

    inline std::string GetStatusPhrase(int code)
    {
        switch (code)
        {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown";
        }
    }

    inline std::string GetHWID()
    {
        HW_PROFILE_INFOA hwProfileInfo;
        if (GetCurrentHwProfileA(&hwProfileInfo))
            return hwProfileInfo.szHwProfileGuid;
        return "Unknown";
    }

    inline std::tuple<std::string, std::string, std::string> GetGameInfo(lua_State* L)
    {
        std::string GameId = "0";
        std::string PlaceId = "0";
        std::string JobId = "0";

        if (!L) return { GameId, PlaceId, JobId };

        int top = lua_gettop(L);

        lua_getglobal(L, "game");
        if (lua_isuserdata(L, -1))
        {
            lua_getfield(L, -1, "GameId");
            if (lua_isstring(L, -1)) {
                GameId = lua_tostring(L, -1);
            }
            else if (lua_isnumber(L, -1)) {
                GameId = std::to_string(static_cast<int>(lua_tonumber(L, -1)));
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "PlaceId");
            if (lua_isstring(L, -1)) {
                PlaceId = lua_tostring(L, -1);
            }
            else if (lua_isnumber(L, -1)) {
                PlaceId = std::to_string(static_cast<int>(lua_tonumber(L, -1)));
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "JobId");
            if (lua_isstring(L, -1)) {
                JobId = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }

        lua_settop(L, top);
        return { GameId, PlaceId, JobId };
    }

    inline bool IsUrlBlocked(const std::string& url)
    {
        for (const auto& blocked : BlockedSites)
        {
            if (url.find(blocked) != std::string::npos)
                return true;
        }
        return false;
    }

    std::string HttpGetSync(const std::string& Url)
    {
        if (Url.find("http://") != 0 && Url.find("https://") != 0)
            return "";

        if (IsUrlBlocked(Url))
            return "";

        std::wstring scheme, host, path;
        INTERNET_PORT port;
        if (!Utils::ParseUrl(Url, scheme, host, path, port))
            return "";

        bool isHttps = (scheme == L"https");

        HINTERNET hSession = WinHttpOpen(L"Vicna 3.0.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) return "";

        WinHttpSetTimeouts(hSession, 30000, 30000, 60000, 60000);

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::wstring headers = L"User-Agent: Vicna 3.0.0\r\n";
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::string responseBody;
        DWORD dwDownloaded = 0;
        do {
            DWORD dwAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable) || dwAvailable == 0)
                break;

            std::vector<char> buffer(dwAvailable + 1, 0);
            if (WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwDownloaded))
                responseBody.append(buffer.data(), dwDownloaded);

        } while (dwDownloaded > 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return responseBody;
    }

    int HttpGet(lua_State* L)
    {
        std::string Url;

        if (lua_isstring(L, 2)) {
            Url = lua_tostring(L, 2);
        }
        else if (lua_isstring(L, 1)) {
            Url = lua_tostring(L, 1);
        }
        else {
            lua_pushnil(L);
            lua_pushstring(L, "HttpGet: expected URL as string");
            return 2;
        }

        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            lua_pushnil(L);
            lua_pushstring(L, "HttpGet: invalid protocol (expected http:// or https://)");
            return 2;
        }

        if (IsUrlBlocked(Url)) {
            lua_pushnil(L);
            lua_pushstring(L, "HttpGet: blocked URL");
            return 2;
        }

        auto [GameId, PlaceId, JobId] = GetGameInfo(L);
        std::string HWID = GetHWID();

        return Yielding::YieldExecution(L, [Url, GameId, PlaceId, JobId, HWID]() -> std::function<int(lua_State*)>
            {
                std::wstring scheme, host, path;
                INTERNET_PORT port;

                if (!Utils::ParseUrl(Url, scheme, host, path, port)) {
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: invalid URL format");
                        return 2;
                        };
                }

                bool isHttps = (scheme == L"https");

                HINTERNET hSession = WinHttpOpen(L"Vicna 3.0.0",
                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, 0);

                if (!hSession) {
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: failed to initialize WinHTTP");
                        return 2;
                        };
                }

                WinHttpSetTimeouts(hSession, 30000, 30000, 60000, 60000);

                HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
                if (!hConnect) {
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: failed to connect");
                        return 2;
                        };
                }

                DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                    NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);

                if (!hRequest) {
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: failed to create request");
                        return 2;
                        };
                }

                std::wstring headers;
                headers += L"User-Agent: Vicna 3.0.0\r\n";
                headers += L"Roblox-Session-Id: {\"GameId\":\"" + Utils::StringToWString(JobId) +
                    L"\",\"PlaceId\":\"" + Utils::StringToWString(PlaceId) + L"\"}\r\n";
                headers += L"Roblox-Place-Id: " + Utils::StringToWString(PlaceId) + L"\r\n";
                headers += L"Roblox-Game-Id: " + Utils::StringToWString(GameId) + L"\r\n";
                headers += L"Exploit-Guid: " + Utils::StringToWString(HWID) + L"\r\n";
                headers += L"Accept: */*\r\n";

                WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

                if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: failed to send request");
                        return 2;
                        };
                }

                if (!WinHttpReceiveResponse(hRequest, NULL)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: failed to receive response");
                        return 2;
                        };
                }

                std::string responseBody;
                DWORD dwDownloaded = 0;
                do {
                    DWORD dwAvailable = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable) || dwAvailable == 0)
                        break;

                    std::vector<char> buffer(dwAvailable + 1, 0);
                    if (WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwDownloaded))
                        responseBody.append(buffer.data(), dwDownloaded);

                } while (dwDownloaded > 0);

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);

                return [responseBody](lua_State* L) -> int {
                    if (responseBody.empty()) {
                        lua_pushnil(L);
                        lua_pushstring(L, "HttpGet: empty response");
                        return 2;
                    }

                    lua_pushlstring(L, responseBody.data(), responseBody.size());
                    return 1;
                    };
            });
    }

    int HttpPost(lua_State* L)
    {
        std::string Url;
        std::string Body;
        std::string ContentType = "application/json";

        if (lua_isstring(L, 1)) {
            Url = lua_tostring(L, 1);
        }
        else if (lua_isuserdata(L, 1)) {
            luaL_checkstring(L, 2);
            Url = lua_tostring(L, 2);
            if (lua_isstring(L, 3))
                Body = lua_tostring(L, 3);
            if (lua_isstring(L, 4))
                ContentType = lua_tostring(L, 4);
        }
        else {
            luaL_argerror(L, 1, "Invalid argument");
            return 0;
        }

        if (lua_isstring(L, 2) && lua_gettop(L) >= 2 && lua_isstring(L, 1)) {
            Body = lua_tostring(L, 2);
            if (lua_isstring(L, 3))
                ContentType = lua_tostring(L, 3);
        }

        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            luaL_argerror(L, 1, "Invalid protocol (expected 'http://' or 'https://')");
            return 0;
        }

        if (IsUrlBlocked(Url)) {
            luaL_argerror(L, 1, "Blocked URL");
            return 0;
        }

        auto [GameId, PlaceId, JobId] = GetGameInfo(L);
        std::string HWID = GetHWID();

        return Yielding::YieldExecution(L, [Url, Body, ContentType, GameId, PlaceId, JobId, HWID]() -> std::function<int(lua_State*)>
            {
                std::wstring scheme, host, path;
                INTERNET_PORT port;

                if (!Utils::ParseUrl(Url, scheme, host, path, port)) {
                    return [](lua_State* L) -> int {
                        luaL_error(L, "HttpPost: invalid URL format");
                        return 0;
                        };
                }

                bool isHttps = (scheme == L"https");

                HINTERNET hSession = WinHttpOpen(L"Vicna 3.0.0",
                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, 0);

                if (!hSession) {
                    return [](lua_State* L) -> int {
                        luaL_error(L, "HttpPost: failed to initialize WinHTTP");
                        return 0;
                        };
                }

                WinHttpSetTimeouts(hSession, 30000, 30000, 60000, 60000);

                HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
                if (!hConnect) {
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        luaL_error(L, "HttpPost: failed to connect");
                        return 0;
                        };
                }

                DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                    NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);

                if (!hRequest) {
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        luaL_error(L, "HttpPost: failed to create request");
                        return 0;
                        };
                }

                std::wstring headers;
                headers += L"User-Agent: Vicna 3.0.0\r\n";
                headers += L"Content-Type: " + Utils::StringToWString(ContentType) + L"\r\n";
                headers += L"Roblox-Session-Id: {\"GameId\":\"" + Utils::StringToWString(JobId) +
                    L"\",\"PlaceId\":\"" + Utils::StringToWString(PlaceId) + L"\"}\r\n";
                headers += L"Exploit-Guid: " + Utils::StringToWString(HWID) + L"\r\n";

                WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

                if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    Body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)Body.c_str(),
                    Body.empty() ? 0 : (DWORD)Body.size(),
                    Body.empty() ? 0 : (DWORD)Body.size(), 0)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        luaL_error(L, "HttpPost: failed to send request");
                        return 0;
                        };
                }

                if (!WinHttpReceiveResponse(hRequest, NULL)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        luaL_error(L, "HttpPost: failed to receive response");
                        return 0;
                        };
                }

                DWORD dwStatusCode = 0;
                DWORD dwSize = sizeof(dwStatusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

                std::string responseBody;
                DWORD dwDownloaded = 0;
                do {
                    DWORD dwAvailable = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable) || dwAvailable == 0)
                        break;

                    std::vector<char> buffer(dwAvailable + 1, 0);
                    if (WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwDownloaded))
                        responseBody.append(buffer.data(), dwDownloaded);

                } while (dwDownloaded > 0);

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);

                return [responseBody, dwStatusCode](lua_State* L) -> int {
                    if (dwStatusCode >= 400) {
                        luaL_error(L, ("HttpPost: HTTP error " + std::to_string(dwStatusCode)).c_str());
                        return 0;
                    }

                    lua_pushlstring(L, responseBody.data(), responseBody.size());
                    return 1;
                    };
            });
    }

    int request(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);

        lua_getfield(L, 1, "Url");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "Success");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "StatusCode");
            lua_pushstring(L, "Missing Url");
            lua_setfield(L, -2, "StatusMessage");
            lua_pushstring(L, "");
            lua_setfield(L, -2, "Body");
            lua_newtable(L);
            lua_setfield(L, -2, "Headers");
            lua_newtable(L);
            lua_setfield(L, -2, "Cookies");
            return 1;
        }
        std::string Url = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (Url.find("http://") != 0 && Url.find("https://") != 0) {
            lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "Success");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "StatusCode");
            lua_pushstring(L, "Invalid protocol");
            lua_setfield(L, -2, "StatusMessage");
            return 1;
        }

        if (IsUrlBlocked(Url)) {
            lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "Success");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "StatusCode");
            lua_pushstring(L, "Blocked URL");
            lua_setfield(L, -2, "StatusMessage");
            return 1;
        }

        RequestMethods Method = H_GET;
        lua_getfield(L, 1, "Method");
        if (lua_isstring(L, -1)) {
            std::string MethodStr = Utils::ToLower(lua_tostring(L, -1));
            auto it = RequestMethodMap.find(MethodStr);
            if (it != RequestMethodMap.end()) {
                Method = it->second;
            }
        }
        lua_pop(L, 1);

        std::map<std::string, std::string> Headers;
        lua_getfield(L, 1, "Headers");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    std::string Key = lua_tostring(L, -2);
                    if (_stricmp(Key.c_str(), "Content-Length") != 0) {
                        Headers[Key] = lua_tostring(L, -1);
                    }
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        std::map<std::string, std::string> Cookies;
        lua_getfield(L, 1, "Cookies");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    Cookies[lua_tostring(L, -2)] = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        std::string Body;
        lua_getfield(L, 1, "Body");
        if (lua_isstring(L, -1)) {
            Body = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        auto [GameId, PlaceId, JobId] = GetGameInfo(L);
        std::string HWID = GetHWID();

        if (Headers.find("User-Agent") == Headers.end())
            Headers["User-Agent"] = "Vicna 3.0.0";
        if (Headers.find("Roblox-Session-Id") == Headers.end())
            Headers["Roblox-Session-Id"] = "{\"GameId\":\"" + JobId + "\",\"PlaceId\":\"" + PlaceId + "\"}";
        if (Headers.find("Exploit-Guid") == Headers.end())
            Headers["Exploit-Guid"] = HWID;

        return Yielding::YieldExecution(L, [Url, Method, Headers, Cookies, Body]() -> std::function<int(lua_State*)>
            {
                std::wstring scheme, host, path;
                INTERNET_PORT port;

                if (!Utils::ParseUrl(Url, scheme, host, path, port)) {
                    return [](lua_State* L) -> int {
                        lua_newtable(L);
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, "Invalid URL format");
                        lua_setfield(L, -2, "StatusMessage");
                        return 1;
                        };
                }

                bool isHttps = (scheme == L"https");

                HINTERNET hSession = WinHttpOpen(L"Vicna 3.0.0",
                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, 0);

                if (!hSession) {
                    return [](lua_State* L) -> int {
                        lua_newtable(L);
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, "Failed to initialize WinHTTP");
                        lua_setfield(L, -2, "StatusMessage");
                        return 1;
                        };
                }

                WinHttpSetTimeouts(hSession, 30000, 30000, 60000, 60000);

                HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
                if (!hConnect) {
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_newtable(L);
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, "Failed to connect");
                        lua_setfield(L, -2, "StatusMessage");
                        return 1;
                        };
                }

                std::wstring wMethod;
                switch (Method) {
                case H_GET: wMethod = L"GET"; break;
                case H_HEAD: wMethod = L"HEAD"; break;
                case H_POST: wMethod = L"POST"; break;
                case H_PUT: wMethod = L"PUT"; break;
                case H_DELETE: wMethod = L"DELETE"; break;
                case H_OPTIONS: wMethod = L"OPTIONS"; break;
                case H_PATCH: wMethod = L"PATCH"; break;
                default: wMethod = L"GET"; break;
                }

                DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), path.c_str(),
                    NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);

                if (!hRequest) {
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_newtable(L);
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, "Failed to create request");
                        lua_setfield(L, -2, "StatusMessage");
                        return 1;
                        };
                }

                std::wstring headers;
                for (const auto& header : Headers) {
                    headers += Utils::StringToWString(header.first) + L": " +
                        Utils::StringToWString(header.second) + L"\r\n";
                }

                if (!Cookies.empty()) {
                    std::string cookieHeader = "Cookie: ";
                    bool first = true;
                    for (const auto& cookie : Cookies) {
                        if (!first) cookieHeader += "; ";
                        cookieHeader += cookie.first + "=" + cookie.second;
                        first = false;
                    }
                    headers += Utils::StringToWString(cookieHeader) + L"\r\n";
                }

                WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

                if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    Body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)Body.c_str(),
                    Body.empty() ? 0 : (DWORD)Body.size(),
                    Body.empty() ? 0 : (DWORD)Body.size(), 0)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_newtable(L);
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, "Failed to send request");
                        lua_setfield(L, -2, "StatusMessage");
                        return 1;
                        };
                }

                if (!WinHttpReceiveResponse(hRequest, NULL)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return [](lua_State* L) -> int {
                        lua_newtable(L);
                        lua_pushboolean(L, 0);
                        lua_setfield(L, -2, "Success");
                        lua_pushinteger(L, 0);
                        lua_setfield(L, -2, "StatusCode");
                        lua_pushstring(L, "Failed to receive response");
                        lua_setfield(L, -2, "StatusMessage");
                        return 1;
                        };
                }

                DWORD dwStatusCode = 0;
                DWORD dwSize = sizeof(dwStatusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

                wchar_t statusBuffer[256] = {};
                dwSize = sizeof(statusBuffer);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT,
                    WINHTTP_HEADER_NAME_BY_INDEX, statusBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX);
                std::string statusMessage = Utils::WStringToString(statusBuffer);

                std::map<std::string, std::string> responseHeaders;
                dwSize = 0;
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                    WINHTTP_HEADER_NAME_BY_INDEX, NULL, &dwSize, WINHTTP_NO_HEADER_INDEX);

                if (dwSize > 0) {
                    std::vector<wchar_t> headerBuffer(dwSize / sizeof(wchar_t));
                    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, headerBuffer.data(), &dwSize, WINHTTP_NO_HEADER_INDEX)) {

                        std::string headerString = Utils::WStringToString(headerBuffer.data());
                        std::istringstream headerStream(headerString);
                        std::string line;

                        while (std::getline(headerStream, line)) {
                            if (line.empty() || line == "\r") continue;
                            size_t colonPos = line.find(':');
                            if (colonPos != std::string::npos) {
                                std::string key = line.substr(0, colonPos);
                                std::string value = line.substr(colonPos + 1);

                                key.erase(0, key.find_first_not_of(" \t\r\n"));
                                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                                value.erase(0, value.find_first_not_of(" \t\r\n"));
                                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                                responseHeaders[key] = value;
                            }
                        }
                    }
                }

                std::string responseBody;
                DWORD dwDownloaded = 0;
                do {
                    DWORD dwAvailable = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable) || dwAvailable == 0)
                        break;

                    std::vector<char> buffer(dwAvailable + 1, 0);
                    if (WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwDownloaded))
                        responseBody.append(buffer.data(), dwDownloaded);

                } while (dwDownloaded > 0);

                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);

                return [responseBody, dwStatusCode, statusMessage, responseHeaders](lua_State* L) -> int {
                    lua_newtable(L);

                    lua_pushboolean(L, dwStatusCode >= 200 && dwStatusCode < 300 ? 1 : 0);
                    lua_setfield(L, -2, "Success");

                    lua_pushinteger(L, dwStatusCode);
                    lua_setfield(L, -2, "StatusCode");

                    lua_pushstring(L, statusMessage.c_str());
                    lua_setfield(L, -2, "StatusMessage");

                    lua_newtable(L);
                    for (const auto& header : responseHeaders) {
                        lua_pushstring(L, header.second.c_str());
                        lua_setfield(L, -2, header.first.c_str());
                    }
                    lua_setfield(L, -2, "Headers");

                    lua_newtable(L);
                    for (const auto& header : responseHeaders) {
                        if (header.first == "Set-Cookie" || header.first == "set-cookie") {
                            size_t eqPos = header.second.find('=');
                            size_t semiPos = header.second.find(';');
                            if (eqPos != std::string::npos) {
                                std::string cookieName = header.second.substr(0, eqPos);
                                std::string cookieValue = header.second.substr(eqPos + 1,
                                    semiPos != std::string::npos ? semiPos - eqPos - 1 : std::string::npos);
                                lua_pushstring(L, cookieValue.c_str());
                                lua_setfield(L, -2, cookieName.c_str());
                            }
                        }
                    }
                    lua_setfield(L, -2, "Cookies");

                    lua_pushlstring(L, responseBody.data(), responseBody.size());
                    lua_setfield(L, -2, "Body");

                    return 1;
                    };
            });
    }

    int GetObjects(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TSTRING);

        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "InsertService");
        lua_call(L, 2, 1);
        lua_remove(L, -2);

        lua_getfield(L, -1, "LoadLocalAsset");
        lua_pushvalue(L, -2);
        lua_pushvalue(L, 2);
        lua_pcall(L, 2, 1, 0);

        if (lua_type(L, -1) == LUA_TSTRING)
            luaL_error(L, lua_tostring(L, -1));

        lua_createtable(L, 1, 0);
        lua_pushvalue(L, -2);
        lua_rawseti(L, -2, 1);

        lua_remove(L, -3);
        lua_remove(L, -2);

        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        ::Utils::AddFunction(L, "request", Http::request);
        ::Utils::AddFunction(L, "http_request", Http::request);
        ::Utils::AddFunction(L, "syn_request", Http::request);

        lua_newtable(L);
        ::Utils::AddTableFunction(L, "request", Http::request);
        //::Utils::AddTableFunction(L, "get", Http::HttpGet);
        //::Utils::AddTableFunction(L, "post", Http::HttpPost);
        lua_setglobal(L, "http");

        ::Utils::AddFunction(L, "HttpGet", Http::HttpGet);
    }
}
