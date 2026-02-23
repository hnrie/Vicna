#pragma once

#define lua_torawuserdata(L, idx) lua_touserdata(L, idx)

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lualib.h>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <bitset>

inline void luaL_trimstack(lua_State* L, int n) {
    lua_settop(L, n);
}

namespace Instance
{
    inline int getinstances(lua_State* L)
    {
        luaL_trimstack(L, 0);

        lua_pushvalue(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(Roblox::Uintptr_TPushInstance)); // possibly wrong pushinstance
        lua_rawget(L, -2);

        if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_pushnil(L); return 1; };

        lua_newtable(L);

        int index = 0;

        lua_pushnil(L);
        while (lua_next(L, -3) != 0) {

            if (!lua_isnil(L, -1)) {
                lua_getglobal(L, "typeof");
                lua_pushvalue(L, -2);
                lua_pcall(L, 1, 1, 0);

                std::string type = lua_tostring(L, -1);
                lua_pop(L, 1);

                if (type == "Instance") {
                    lua_pushinteger(L, ++index);

                    lua_pushvalue(L, -2);
                    lua_settable(L, -5);
                }
            }

            lua_pop(L, 1);
        }

        lua_remove(L, -2);

        return 1;
    }

    inline int getnilinstances(lua_State* L)
    {
        luaL_trimstack(L, 0);

        lua_pushvalue(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(Roblox::Uintptr_TPushInstance)); // possibly wrong pushinstance
        lua_rawget(L, -2);

        if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_pushnil(L); return 1; };

        lua_newtable(L);

        int index = 0;

        lua_pushnil(L);
        while (lua_next(L, -3) != 0) {

            if (!lua_isnil(L, -1)) {
                lua_getglobal(L, "typeof");
                lua_pushvalue(L, -2);
                lua_pcall(L, 1, 1, 0);

                std::string type = lua_tostring(L, -1);
                lua_pop(L, 1);

                if (type == "Instance") {
                    lua_getfield(L, -1, "Parent");
                    int parentType = lua_type(L, -1);
                    lua_pop(L, 1);

                    if (parentType == LUA_TNIL) {
                        lua_pushinteger(L, ++index);

                        lua_pushvalue(L, -2);
                        lua_settable(L, -5);
                    }
                }
            }

            lua_pop(L, 1);
        }

        lua_remove(L, -2);

        return 1;
    }

    inline int isnetworkowner(lua_State* L)
    {
        if (!L || !lua_isuserdata(L, 1)) {
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_getfield(L, 1, "ClassName");
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_pushboolean(L, 0);
            return 1;
        }

        const char* cn = lua_tostring(L, -1);
        if (!cn) {
            lua_pop(L, 1);
            lua_pushboolean(L, 0);
            return 1;
        }
        lua_pop(L, 1);

        if (strcmp(cn, "BasePart") != 0 && strcmp(cn, "Part") != 0 &&
            strcmp(cn, "MeshPart") != 0 && strcmp(cn, "UnionOperation") != 0) {
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_getfield(L, 1, "GetNetworkOwner");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_pushvalue(L, 1);
        if (lua_pcall(L, 1, 1, 0) != 0) {
            lua_pop(L, 1);
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "Players");

        if (lua_pcall(L, 2, 1, 0) != 0) {
            lua_pop(L, 3);
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_getfield(L, -1, "LocalPlayer");
        bool is_owner = lua_rawequal(L, -1, -4);

        lua_pop(L, 4);
        lua_pushboolean(L, is_owner ? 1 : 0);
        return 1;
    }

    inline int getsimulationradius(lua_State* L)
    {
        if (!L) {
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            return 2;
        }

        lua_getglobal(L, "game");
        if (!lua_isuserdata(L, -1)) {
            lua_pop(L, 1);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            return 2;
        }

        lua_getfield(L, -1, "GetService");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            return 2;
        }

        lua_pushvalue(L, -2);
        lua_pushstring(L, "Players");

        if (lua_pcall(L, 2, 1, 0) != 0) {
            lua_pop(L, 2);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            return 2;
        }

        lua_getfield(L, -1, "LocalPlayer");
        if (lua_isuserdata(L, -1)) {
            lua_pushnumber(L, 1000.0);
            lua_pushnumber(L, 1000.0);
            lua_remove(L, -3);
            lua_remove(L, -3);
            return 2;
        }

        lua_pop(L, 2);
        lua_pushnumber(L, 0);
        lua_pushnumber(L, 0);
        return 2;
    }

    inline int setsimulationradius(lua_State* L)
    {
        double radius = luaL_checknumber(L, 1);
        double maxRadius = luaL_optnumber(L, 2, radius);

        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "Players");

        if (lua_pcall(L, 2, 1, 0) == 0) {
            lua_getfield(L, -1, "LocalPlayer");
            if (lua_isuserdata(L, -1)) {
                lua_pushnumber(L, radius);
                lua_setfield(L, -2, "SimulationRadius");

                lua_pushnumber(L, maxRadius);
                lua_setfield(L, -2, "MaximumSimulationRadius");
            }
        }

        return 0;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "getnilinstances", getnilinstances);
        Utils::AddFunction(L, "getinstances", getinstances);
        Utils::AddFunction(L, "isnetworkowner", isnetworkowner);
        Utils::AddFunction(L, "getsimulationradius", getsimulationradius);
        Utils::AddFunction(L, "setsimulationradius", setsimulationradius);
    }
}