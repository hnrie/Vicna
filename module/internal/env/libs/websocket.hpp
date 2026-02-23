#pragma once
#include <map>
#include <lmem.h>
#include <lgc.h>
#include <lualib.h>
#include <internal/utils.hpp>

namespace Websocket {
    inline void enable_websocket_creation() {
        //*(uint8_t*)Offsets::WebSocketServiceEnableClientCreation = 1;
    }

    inline int send(lua_State* l) {
        lua_getfield(l, 1, "_raw");
        if (!lua_istable(l, -1) && !lua_isuserdata(l, -1)) {
            lua_pop(l, 1);
            return 0;
        }
        lua_getfield(l, -1, "Send");
        lua_pushvalue(l, -2);
        const char* msg = lua_tostring(l, 2);
        if (!msg) msg = "";
        lua_pushstring(l, msg);
        lua_pcall(l, 2, 0, 0);
        lua_pop(l, 1);
        return 0;
    }

    inline int close(lua_State* l) {
        lua_getfield(l, 1, "_raw");
        if (!lua_istable(l, -1) && !lua_isuserdata(l, -1)) {
            lua_pop(l, 1);
            return 0;
        }
        lua_getfield(l, -1, "Close");
        lua_pushvalue(l, -2);
        lua_pcall(l, 1, 0, 0);
        lua_pop(l, 1);
        return 0;
    }

    inline int event_connect(lua_State* l) {
        int top = lua_gettop(l);
        if (!lua_istable(l, 1) && !lua_isuserdata(l, 1))
            return 0;

        if (!lua_isfunction(l, 2))
            return 0;

        lua_getfield(l, 1, "_handlers");
        if (!lua_istable(l, -1)) {
            lua_pop(l, 1);
            lua_newtable(l);
            lua_pushvalue(l, -1);
            lua_setfield(l, 1, "_handlers");
        }
        int len = lua_objlen(l, -1);
        lua_pushvalue(l, 2);
        lua_rawseti(l, -2, len + 1);

        lua_getfield(l, 1, "_eventname");
        const char* eventName = lua_tostring(l, -1);
        lua_pop(l, 1);

        if (eventName && strcmp(eventName, "OnMessage") == 0) {
            lua_getfield(l, 1, "_conn");
            if (lua_istable(l, -1)) {
                int connIndex = lua_absindex(l, -1);
                lua_getfield(l, connIndex, "_pending_messages");
                if (lua_istable(l, -1)) {
                    int pendingLen = lua_objlen(l, -1);
                    for (int i = 1; i <= pendingLen; i++) {
                        lua_rawgeti(l, -1, i);
                        size_t msgLen = 0;
                        const char* msg = lua_tolstring(l, -1, &msgLen);
                        lua_pop(l, 1);
                        if (!msg) continue;

                        lua_pushvalue(l, 2);
                        lua_pushlstring(l, msg, msgLen);
                        if (lua_pcall(l, 1, 0, 0) != LUA_OK) {
                            lua_pop(l, 1);
                        }
                    }
                    lua_newtable(l);
                    lua_setfield(l, connIndex, "_pending_messages");
                }
                lua_pop(l, 1);
            }
            lua_pop(l, 1);
        }

        lua_pop(l, 1);
        lua_settop(l, top);
        return 0;
    }

    static bool is_valid_websocket_url(const char* url)
    {
        if (!url) return false;
        std::string s(url);
        if (s.empty()) return false;
        if (s.find_first_of(" \t\r\n") != std::string::npos)
            return false;
        if (!(s.rfind("ws://", 0) == 0 || s.rfind("wss://", 0) == 0))
            return false;
        size_t scheme_end = s.find("://") + 3;
        if (scheme_end >= s.size())
            return false;
        if (s[scheme_end] == '/')
            return false;
        return true;
    }

    static bool get_message_payload(lua_State* l, int idx, const char** outData, size_t* outLen)
    {
        if (lua_isstring(l, idx))
        {
            *outData = lua_tolstring(l, idx, outLen);
            return *outData != nullptr;
        }

        if (lua_istable(l, idx) || lua_isuserdata(l, idx))
        {
            int top = lua_gettop(l);
            lua_getfield(l, idx, "Data");
            if (lua_isstring(l, -1))
            {
                *outData = lua_tolstring(l, -1, outLen);
                lua_settop(l, top);
                return *outData != nullptr;
            }
            lua_settop(l, top);
        }

        return false;
    }

    inline int connect(lua_State* l) {
        const char* url = luaL_checkstring(l, 1);
        if (!is_valid_websocket_url(url))
        {
            luaL_error(l, "invalid WebSocket URL");
            return 0;
        }
        lua_getglobal(l, "game");
        lua_getfield(l, -1, "GetService");
        lua_pushvalue(l, -2);
        lua_pushstring(l, "WebSocketService");
        lua_call(l, 2, 1);
        lua_getfield(l, -1, "CreateClient");
        lua_pushvalue(l, -2);
        lua_pushstring(l, url);
        lua_call(l, 2, 1);
        int raw_index = lua_absindex(l, lua_gettop(l));
        lua_newtable(l);
        int conn_index = lua_absindex(l, lua_gettop(l));
        lua_pushstring(l, "_raw");
        lua_pushvalue(l, raw_index);
        lua_settable(l, conn_index);
        lua_newtable(l);
        lua_setfield(l, conn_index, "_connections");
        lua_newtable(l);
        lua_setfield(l, conn_index, "_pending_messages");
        lua_pushstring(l, "OnMessage");
        lua_newtable(l);
        lua_newtable(l);
        lua_setfield(l, -2, "_handlers");
        lua_pushstring(l, "OnMessage");
        lua_setfield(l, -2, "_eventname");
        lua_pushvalue(l, conn_index);
        lua_setfield(l, -2, "_conn");
        lua_pushstring(l, "Connect");
        lua_pushcclosure(l, event_connect, nullptr, 0);
        lua_settable(l, -3);
        lua_settable(l, conn_index);
        lua_pushstring(l, "OnClose");
        lua_newtable(l);
        lua_newtable(l);
        lua_setfield(l, -2, "_handlers");
        lua_pushstring(l, "OnClose");
        lua_setfield(l, -2, "_eventname");
        lua_pushvalue(l, conn_index);
        lua_setfield(l, -2, "_conn");
        lua_pushstring(l, "Connect");
        lua_pushcclosure(l, event_connect, nullptr, 0);
        lua_settable(l, -3);
        lua_settable(l, conn_index);
        lua_pushstring(l, "OnOpen");
        lua_newtable(l);
        lua_newtable(l);
        lua_setfield(l, -2, "_handlers");
        lua_pushstring(l, "OnOpen");
        lua_setfield(l, -2, "_eventname");
        lua_pushvalue(l, conn_index);
        lua_setfield(l, -2, "_conn");
        lua_pushstring(l, "Connect");
        lua_pushcclosure(l, event_connect, nullptr, 0);
        lua_settable(l, -3);
        lua_settable(l, conn_index);
        lua_pushstring(l, "Send");
        lua_pushcclosure(l, send, nullptr, 0);
        lua_settable(l, conn_index);
        lua_pushstring(l, "Close");
        lua_pushcclosure(l, close, nullptr, 0);
        lua_settable(l, conn_index);
        lua_pushvalue(l, raw_index);
        lua_getfield(l, -1, "MessageReceived");
        if (lua_istable(l, -1) || lua_isuserdata(l, -1)) {
            lua_getfield(l, -1, "Connect");
            lua_pushvalue(l, -2);
            lua_pushvalue(l, conn_index);
            lua_pushcclosure(l, [](lua_State* l) -> int {
                int top = lua_gettop(l);
                size_t data_len = 0;
                const char* data = nullptr;
                if (!get_message_payload(l, 1, &data, &data_len))
                    get_message_payload(l, 2, &data, &data_len);
                if (!data) { data = ""; data_len = 0; }
                lua_pushvalue(l, lua_upvalueindex(1));
                lua_getfield(l, -1, "OnMessage");
                lua_getfield(l, -1, "_handlers");
                if (lua_istable(l, -1)) {
                    int len = lua_objlen(l, -1);
                    if (len <= 0) {
                        lua_pushvalue(l, lua_upvalueindex(1));
                        lua_getfield(l, -1, "_pending_messages");
                        if (lua_istable(l, -1)) {
                            int pendingLen = lua_objlen(l, -1);
                            lua_pushlstring(l, data, data_len);
                            lua_rawseti(l, -2, pendingLen + 1);
                        }
                        lua_pop(l, 2);
                    }
                    else {
                        for (int i = 1; i <= len; i++) {
                            lua_rawgeti(l, -1, i);
                            if (lua_isfunction(l, -1)) {
                                lua_pushlstring(l, data, data_len);
                                if (lua_pcall(l, 1, 0, 0) != LUA_OK) {
                                    lua_pop(l, 1);
                                }
                            }
                            else {
                                lua_pop(l, 1);
                            }
                        }
                    }
                }
                lua_settop(l, top);
                return 0;
                }, nullptr, 1);
            lua_call(l, 2, 1);
            if (!lua_isnil(l, -1)) {
                lua_pushvalue(l, conn_index);
                lua_getfield(l, -1, "_connections");
                if (lua_istable(l, -1)) {
                    int len = lua_objlen(l, -1);
                    lua_pushvalue(l, -3);
                    lua_rawseti(l, -2, len + 1);
                }
                lua_pop(l, 2);
            }
            lua_pop(l, 1);
        }
        lua_pop(l, 2);
        lua_pushvalue(l, raw_index);
        lua_getfield(l, -1, "Opened");
        if (lua_istable(l, -1) || lua_isuserdata(l, -1)) {
            lua_getfield(l, -1, "Connect");
            lua_pushvalue(l, -2);
            lua_pushvalue(l, conn_index);
            lua_pushcclosure(l, [](lua_State* l) -> int {
                int top = lua_gettop(l);
                lua_pushvalue(l, lua_upvalueindex(1));
                lua_getfield(l, -1, "OnOpen");
                lua_getfield(l, -1, "_handlers");
                if (lua_istable(l, -1)) {
                    int len = lua_objlen(l, -1);
                    for (int i = 1; i <= len; i++) {
                        lua_rawgeti(l, -1, i);
                        if (lua_isfunction(l, -1)) {
                            if (lua_pcall(l, 0, 0, 0) != LUA_OK) {
                                lua_pop(l, 1);
                            }
                        }
                        else {
                            lua_pop(l, 1);
                        }
                    }
                }
                lua_settop(l, top);
                return 0;
                }, nullptr, 1);
            lua_call(l, 2, 1);
            if (!lua_isnil(l, -1)) {
                lua_pushvalue(l, conn_index);
                lua_getfield(l, -1, "_connections");
                if (lua_istable(l, -1)) {
                    int len = lua_objlen(l, -1);
                    lua_pushvalue(l, -3);
                    lua_rawseti(l, -2, len + 1);
                }
                lua_pop(l, 2);
            }
            lua_pop(l, 1);
        }
        lua_pop(l, 2);
        lua_pushvalue(l, raw_index);
        lua_getfield(l, -1, "Closed");
        if (lua_istable(l, -1) || lua_isuserdata(l, -1)) {
            lua_getfield(l, -1, "Connect");
            lua_pushvalue(l, -2);
            lua_pushvalue(l, conn_index);
            lua_pushcclosure(l, [](lua_State* l) -> int {
                int top = lua_gettop(l);
                lua_pushvalue(l, lua_upvalueindex(1));
                lua_getfield(l, -1, "OnClose");
                lua_getfield(l, -1, "_handlers");
                if (lua_istable(l, -1)) {
                    int len = lua_objlen(l, -1);
                    for (int i = 1; i <= len; i++) {
                        lua_rawgeti(l, -1, i);
                        if (lua_isfunction(l, -1)) {
                            if (lua_pcall(l, 0, 0, 0) != LUA_OK) {
                                lua_pop(l, 1);
                            }
                        }
                        else {
                            lua_pop(l, 1);
                        }
                    }
                }
                lua_settop(l, top);
                return 0;
                }, nullptr, 1);
            lua_call(l, 2, 1);
            if (!lua_isnil(l, -1)) {
                lua_pushvalue(l, conn_index);
                lua_getfield(l, -1, "_connections");
                if (lua_istable(l, -1)) {
                    int len = lua_objlen(l, -1);
                    lua_pushvalue(l, -3);
                    lua_rawseti(l, -2, len + 1);
                }
                lua_pop(l, 2);
            }
            lua_pop(l, 1);
        }
        lua_pop(l, 2);
        lua_pushvalue(l, conn_index);
        return 1;
    }

    inline void register_library(lua_State* l) {
        enable_websocket_creation();
        lua_newtable(l);
        Utils::AddTableFunction(l, "connect", Websocket::connect);
        lua_setglobal(l, "WebSocket");
    }
}
