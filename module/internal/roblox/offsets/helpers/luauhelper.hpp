#pragma once

#include <lstate.h>
#include <lobject.h>
#include <lfunc.h>
#include <lgc.h>

inline Closure* lua_toclosure(lua_State* L, int idx)
{
    if (!L) return nullptr;

    if (!lua_isfunction(L, idx)) {
        return nullptr;
    }

    int abs_idx = idx;
    if (idx < 0 && idx > LUA_REGISTRYINDEX) {
        abs_idx = lua_gettop(L) + idx + 1;
    }

    if (abs_idx < 1 || abs_idx > lua_gettop(L)) {
        return nullptr;
    }

    StkId stack_pos = L->base + (abs_idx - 1);

    if (stack_pos < L->base || stack_pos >= L->top) {
        return nullptr;
    }

    if (stack_pos->tt != LUA_TFUNCTION) {
        return nullptr;
    }

    return (Closure*)stack_pos->value.gc;
}

inline void lua_clonecfunction(lua_State* L, int idx)
{
    lua_clonefunction(L, idx);
}