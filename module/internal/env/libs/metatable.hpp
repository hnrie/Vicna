#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lobject.h>
#include <lfunc.h>
#include <lstring.h>
#include <ltable.h>
#include <lapi.h>
#include <lgc.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

namespace Metatable
{
    inline int isreadonly(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua_pushboolean(L, lua_getreadonly(L, 1));
        return 1;
    }

    inline int setreadonly(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        lua_setreadonly(L, 1, lua_toboolean(L, 2));
        return 0;
    }

    inline int getrawmetatable(lua_State* l) {
        lua_check(l, 1);
        luaL_checkany(l, 1);

        if (!lua_getmetatable(l, 1))
            lua_pushnil(l);

        return 1;
    }

    inline int setrawmetatable(lua_State* l) {
        luaL_checkany(l, 1);
        luaL_checktype(l, 2, LUA_TTABLE);
        lua_setmetatable(l, 1);
        lua_pushvalue(l, 1);
        return 1;
    }

    inline int getnamecallmethod(lua_State* L)
    {
        const char* namecall = lua_namecallatom(L, nullptr);

        if (!namecall)
        {
            lua_pushnil(L);
            return 1;
        }

        lua_pushstring(L, namecall);
        return 1;
    }

    inline int setnamecallmethod(lua_State* L)
    {
        lua_check(L, 1);
        luaL_checktype(L, 1, LUA_TSTRING);

        L->namecall = tsvalue(luaA_toobject(L, 1));

        return 0;
    }

    inline int hookmetamethod(lua_State* L)
    {
        luaL_checkany(L, 1);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TFUNCTION);

        const char* metamethod = lua_tostring(L, 2);

        if (!lua_getmetatable(L, 1)) {
            luaL_argerror(L, 1, "object has no metatable");
        }
        int mt_idx = lua_gettop(L);

        bool wasReadonly = lua_getreadonly(L, mt_idx);
        if (wasReadonly) {
            lua_setreadonly(L, mt_idx, false);
        }

        lua_getfield(L, mt_idx, metamethod);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            if (wasReadonly) lua_setreadonly(L, mt_idx, true);
            luaL_argerror(L, 2, "metamethod does not exist");
        }
        int old_method = lua_gettop(L);

        lua_pushvalue(L, 3);
        lua_setfield(L, mt_idx, metamethod);

        if (wasReadonly) {
            lua_setreadonly(L, mt_idx, true);
        }

        lua_pushvalue(L, old_method);
        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "getrawmetatable", getrawmetatable);
        Utils::AddFunction(L, "setrawmetatable", setrawmetatable);
        Utils::AddFunction(L, "hookmetamethod", hookmetamethod);
        Utils::AddFunction(L, "getnamecallmethod", getnamecallmethod);
        Utils::AddFunction(L, "setnamecallmethod", setnamecallmethod);
        Utils::AddFunction(L, "setreadonly", setreadonly);
        Utils::AddFunction(L, "makereadonly", setreadonly);
        Utils::AddFunction(L, "isreadonly", isreadonly);
    }
}
