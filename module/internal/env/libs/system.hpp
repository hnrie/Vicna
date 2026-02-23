#pragma once

#include <Windows.h>
#include <lstate.h>
#include <string>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

namespace System
{
    inline int setclipboard(lua_State* L)
    {
        if (!lua_isstring(L, 1)) {
            return 0;
        }

        const char* text = lua_tostring(L, 1);
        if (!text) return 0;

        size_t len = strlen(text);
        if (len == 0) return 0;

        if (!OpenClipboard(nullptr)) {
            return 0;
        }

        EmptyClipboard();

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
        if (hMem) {
            char* pMem = (char*)GlobalLock(hMem);
            if (pMem) {
                for (size_t i = 0; i <= len; i++) {
                    pMem[i] = text[i];
                }
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            else {
                GlobalFree(hMem);
            }
        }

        CloseClipboard();
        return 0;
    }

    inline int getclipboard(lua_State* L)
    {
        if (!OpenClipboard(nullptr)) {
            lua_pushstring(L, "");
            return 1;
        }

        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) {
                lua_pushstring(L, pszText);
                GlobalUnlock(hData);
                CloseClipboard();
                return 1;
            }
        }

        CloseClipboard();
        lua_pushstring(L, "");
        return 1;
    }

    inline int messagebox(lua_State* L)
    {
        const char* text = luaL_checkstring(L, 1);
        const char* caption = luaL_checkstring(L, 2);
        int flags = luaL_optinteger(L, 3, MB_OK);

        int result = MessageBoxA(nullptr, text, caption, flags);
        lua_pushinteger(L, result);
        return 1;
    }

    inline int gethwid(lua_State* L)
    {
        HW_PROFILE_INFOA hwProfileInfo;
        if (GetCurrentHwProfileA(&hwProfileInfo)) {
            lua_pushstring(L, hwProfileInfo.szHwProfileGuid);
        }
        else {
            lua_pushstring(L, "Unknown");
        }
        return 1;
    }

    inline int isrbxactive(lua_State* L)
    {
        HWND fg = GetForegroundWindow();
        HWND rbx = FindWindowA(nullptr, "Roblox");

        if (!rbx) {
            rbx = FindWindowA("WINDOWSCLIENT", nullptr);
        }

        bool active = (fg == rbx);
        lua_pushboolean(L, active ? 1 : 0);
        return 1;
    }

    inline int iswindowactive(lua_State* L)
    {
        HWND hwnd = GetForegroundWindow();
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        lua_pushboolean(L, pid == GetCurrentProcessId() ? 1 : 0);
        return 1;
    }

    inline int keypress(lua_State* L)
    {
        int key = luaL_checkinteger(L, 1);
        if (key < 0 || key > 255) {
            return 0;
        }

        keybd_event(static_cast<BYTE>(key), 0, 0, 0);
        return 0;
    }

    inline int keyrelease(lua_State* L)
    {
        int key = luaL_checkinteger(L, 1);
        if (key < 0 || key > 255) {
            return 0;
        }

        keybd_event(static_cast<BYTE>(key), 0, KEYEVENTF_KEYUP, 0);
        return 0;
    }

    inline int mouse1click(lua_State* L)
    {
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(10);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        return 0;
    }

    inline int mouse1press(lua_State* L)
    {
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        return 0;
    }

    inline int mouse1release(lua_State* L)
    {
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        return 0;
    }

    inline int mouse2click(lua_State* L)
    {
        mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
        Sleep(10);
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
        return 0;
    }

    inline int mouse2press(lua_State* L)
    {
        mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
        return 0;
    }

    inline int mouse2release(lua_State* L)
    {
        mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
        return 0;
    }

    inline int mousemoverel(lua_State* L)
    {
        int x = luaL_checkinteger(L, 1);
        int y = luaL_checkinteger(L, 2);

        mouse_event(MOUSEEVENTF_MOVE, x, y, 0, 0);
        return 0;
    }

    inline int mousemoveabs(lua_State* L)
    {
        int x = luaL_checkinteger(L, 1);
        int y = luaL_checkinteger(L, 2);

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        int absX = (x * 65535) / screenWidth;
        int absY = (y * 65535) / screenHeight;

        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE, absX, absY, 0, 0);
        return 0;
    }

    inline int mousescroll(lua_State* L)
    {
        int delta = luaL_checkinteger(L, 1);
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, delta, 0);
        return 0;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "setclipboard", setclipboard);
        Utils::AddFunction(L, "setrbxclipboard", setclipboard);
        Utils::AddFunction(L, "getclipboard", getclipboard);
        Utils::AddFunction(L, "toclipboard", setclipboard);

        Utils::AddFunction(L, "messagebox", messagebox);

        Utils::AddFunction(L, "gethwid", gethwid);
        Utils::AddFunction(L, "isrbxactive", isrbxactive);
        Utils::AddFunction(L, "isgameactive", isrbxactive);
        Utils::AddFunction(L, "iswindowactive", iswindowactive);

        Utils::AddFunction(L, "keypress", keypress);
        Utils::AddFunction(L, "keyrelease", keyrelease);

        Utils::AddFunction(L, "mouse1click", mouse1click);
        Utils::AddFunction(L, "mouse1press", mouse1press);
        Utils::AddFunction(L, "mouse1release", mouse1release);
        Utils::AddFunction(L, "mouse2click", mouse2click);
        Utils::AddFunction(L, "mouse2press", mouse2press);
        Utils::AddFunction(L, "mouse2release", mouse2release);
        Utils::AddFunction(L, "mousemoverel", mousemoverel);
        Utils::AddFunction(L, "mousemoveabs", mousemoveabs);
        Utils::AddFunction(L, "mousescroll", mousescroll);
    }
}