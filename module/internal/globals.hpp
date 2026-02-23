#pragma once

#include <lstate.h>
#include <string>
#include <queue>
#include <functional>
#include <atomic>

#include "internal/roblox/offsets/helpers/luauhelper.hpp"
#include <lualib.h>

typedef void(__fastcall* PushInstanceWeakPtrT)(lua_State* L, std::weak_ptr<uintptr_t>);
static PushInstanceWeakPtrT PushInstanceWeakPtr = (PushInstanceWeakPtrT)Offsets::PushInstance;

static LuaTable* getcurrenv(lua_State* L)
{
    if (L->ci == L->base_ci)
        return L->gt;
    else
        return curr_func(L)->env;
}

static LUAU_NOINLINE TValue* pseudo2addr(lua_State* L, int idx)
{
    api_check(L, lua_ispseudo(idx));
    switch (idx)
    { // pseudo-indices
    case LUA_REGISTRYINDEX:
        return registry(L);
    case LUA_ENVIRONINDEX:
    {
        sethvalue(L, &L->global->pseudotemp, getcurrenv(L));
        return &L->global->pseudotemp;
    }
    case LUA_GLOBALSINDEX:
    {
        sethvalue(L, &L->global->pseudotemp, L->gt);
        return &L->global->pseudotemp;
    }
    default:
    {
        Closure* func = curr_func(L);
        idx = LUA_GLOBALSINDEX - idx;
        return (idx <= func->nupvalues) ? &func->c.upvals[idx - 1] : cast_to(TValue*, luaO_nilobject);
    }
    }
}

static LUAU_FORCEINLINE TValue* index2addr(lua_State* L, int idx)
{
    if (idx > 0)
    {
        TValue* o = L->base + (idx - 1);
        api_check(L, idx <= L->ci->top - L->base);
        if (o >= L->top)
            return cast_to(TValue*, luaO_nilobject);
        else
            return o;
    }
    else if (idx > LUA_REGISTRYINDEX)
    {
        api_check(L, idx != 0 && -idx <= L->top - L->base);
        return L->top + idx;
    }
    else
    {
        return pseudo2addr(L, idx);
    }
}

inline uintptr_t MaxCapabilities = 0xFFFFFFFFFFFFFFFF;

namespace SharedVariables
{
    inline uintptr_t LastDataModel;
    inline lua_State* ExploitThread;
    inline std::vector<std::string> ExecutionRequests;
    inline std::queue<std::function<void()>> YieldQueue;

    inline std::unordered_map<std::string, lua_State*> CommChannels;
    inline std::unordered_map<lua_State*, bool> ParallelStates;
    inline std::unordered_map<uintptr_t, std::vector<lua_State*>> ActorThreads;
}

struct Actor {
    uintptr_t instance;
    lua_State* thread;
    std::string name;
};

struct CommChannel {
    std::string name;
    lua_State* sender;
    lua_State* receiver;
    std::queue<std::string> messages;
};

inline void lua_normalisestack(lua_State* L, int n) {
    int top = lua_gettop(L);
    if (top < n) {
        lua_checkstack(L, n - top);
        for (int i = top; i < n; i++) {
            lua_pushnil(L);
        }
    }
}

inline void lua_check(lua_State* L, int n) {
    if (lua_gettop(L) < n) {
        luaL_error(L, ("expected at least %d arguments, got %d"), n, lua_gettop(L));
    }
}