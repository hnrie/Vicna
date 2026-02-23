#pragma once

#include <internal/globals.hpp>
#include <internal/utils.hpp>

namespace Cache
{
    inline int invalidate(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        const auto rawUserdata = *static_cast<void**>(lua_touserdata(L, 1));

        lua_pushlightuserdata(L, (void*)Roblox::PushInstance);
        lua_gettable(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, reinterpret_cast<void*>(rawUserdata));
        lua_pushnil(L);
        lua_settable(L, -3);

        return 0;
    }

    inline int replace(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TUSERDATA);

        const auto rawUserdata = *static_cast<void**>(lua_touserdata(L, 1));

        lua_pushlightuserdata(L, (void*)Roblox::PushInstance);
        lua_gettable(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, rawUserdata);
        lua_pushvalue(L, 2);
        lua_settable(L, -3);

        return 0;
    }

    inline int iscached(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        const auto rawUserdata = *static_cast<void**>(lua_touserdata(L, 1));

        lua_pushlightuserdata(L, (void*)Roblox::PushInstance);
        lua_gettable(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, rawUserdata);
        lua_gettable(L, -2);

        lua_pushboolean(L, lua_type(L, -1) != LUA_TNIL);

        return 1;
    }

    inline int cloneref(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        auto Ud = lua_touserdata(L, 1);
        auto RawUd = *static_cast<void**>(Ud);

        lua_pushlightuserdata(L, reinterpret_cast<void*>(Roblox::PushInstance));
        lua_rawget(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, RawUd);
        lua_rawget(L, -2);

        lua_pushlightuserdata(L, RawUd);
        lua_pushnil(L);
        lua_rawset(L, -4);

        reinterpret_cast<void(__fastcall*)(lua_State*, void*)>(Roblox::PushInstance)(L, Ud);

        lua_pushlightuserdata(L, RawUd);
        lua_pushvalue(L, -3);
        lua_rawset(L, -5);

        return 1;
    }

    inline int compareinstances(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TUSERDATA);

        uintptr_t instance_one = *reinterpret_cast<uintptr_t*>(lua_touserdata(L, 1));
        uintptr_t instance_two = *reinterpret_cast<uintptr_t*>(lua_touserdata(L, 2));

        lua_pushboolean(L, instance_one == instance_two);

        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        lua_newtable(L);

        Utils::RegisterTableAliases(L, invalidate, { "invalidate", "Invalidate", "clear", "Clear" });
        Utils::RegisterTableAliases(L, iscached, { "iscached", "isCached", "IsCached", "is_cached" });
        Utils::RegisterTableAliases(L, replace, { "replace", "Replace", "set", "Set" });

        lua_setglobal(L, "cache");

        //Utils::AddFunction(L, "cloneref", cloneref);
        Utils::RegisterAliases(L, cloneref, { "cloneref", "clonereference" });
        Utils::AddFunction(L, "compareinstances", compareinstances);
    }
}