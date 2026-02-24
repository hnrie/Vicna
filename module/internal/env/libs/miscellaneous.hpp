#pragma once

#include <Windows.h>
#include <lgc.h>
#include <lfunc.h>
#include <lmem.h>
#include <lstate.h>
#include <lobject.h>
#include <lapi.h>
#include <lstring.h>
#include <lgc.h>
#include <mutex>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

#include <vector>
#include <lz4/lz4.h>
#include <ltable.h>
#include <internal/roblox/scheduler/scheduler.hpp>

#define lua_pushgc(L, n) \
L->top->value.gc = reinterpret_cast<GCObject*>(n); \
L->top->tt = n->tt; \
incr_top(L)

namespace Miscellaneous
{
    inline std::mutex g_ActorMutex;
    inline std::mutex g_CommMutex;
    inline std::unordered_map<std::string, std::queue<std::string>> g_CommChannels;
    inline std::unordered_map<lua_State*, bool> g_ParallelStates;
    inline std::unordered_map<uintptr_t, std::vector<lua_State*>> g_ActorThreads;

    struct getgc_context
    {
        lua_State* state;
        bool include_tables;
        int index;
        int result_index;
    };

    static bool getgc_visit(void* context, lua_Page* page, GCObject* gco)
    {
        (void)page;

        auto* ctx = static_cast<getgc_context*>(context);

        int tt = gco->gch.tt;
        if (tt == LUA_TFUNCTION || tt == LUA_TUSERDATA || (ctx->include_tables && tt == LUA_TTABLE))
        {
            TValue obj;
            switch (tt)
            {
            case LUA_TFUNCTION:
                setclvalue(ctx->state, &obj, gco2cl(gco));
                break;
            case LUA_TUSERDATA:
                setuvalue(ctx->state, &obj, gco2u(gco));
                break;
            default:
                sethvalue(ctx->state, &obj, gco2h(gco));
                break;
            }

            luaA_pushobject(ctx->state, &obj);
            lua_rawseti(ctx->state, ctx->result_index, ++ctx->index);
        }

        return false;
    }

    int identifyexecutor(lua_State* L)
    {
        lua_pushstring(L, "Vicna");
        lua_pushstring(L, "3.0.1");
        return 2;
    }

    int getexecutorname(lua_State* L)
    {
        lua_pushstring(L, "Vicna");
        return 1;
    }

    int getgenv(lua_State* L)
    {
        if (SharedVariables::ExploitThread == L) {
            lua_pushvalue(L, LUA_GLOBALSINDEX);
            return 1;
        }

        lua_rawcheckstack(L, 1);
        luaC_threadbarrier(L);
        luaC_threadbarrier(SharedVariables::ExploitThread);
        lua_pushvalue(SharedVariables::ExploitThread, LUA_GLOBALSINDEX);
        lua_xmove(SharedVariables::ExploitThread, L, 1);

        return 1;
    }

    int getrenv(lua_State* L)
    {
        lua_check(L, 0);

        uintptr_t ScriptContext = TaskScheduler::GetScriptContext(SharedVariables::LastDataModel);
        if (!ScriptContext)
            return false;

        lua_State* roblox_state = TaskScheduler::GetLuaStateForInstance(ScriptContext);

        LuaTable* clone = luaH_clone(L, roblox_state->gt);

        lua_rawcheckstack(L, 1);
        luaC_threadbarrier(L);
        luaC_threadbarrier(roblox_state);

        L->top->value.p = clone;
        L->top->tt = LUA_TTABLE;
        L->top++;

        lua_rawgeti(L, LUA_REGISTRYINDEX, 2);
        lua_setfield(L, -2, ("_G"));
        lua_rawgeti(L, LUA_REGISTRYINDEX, 4);
        lua_setfield(L, -2, ("shared"));
        return 1;
    }

    int getmenv(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        lua_getfield(L, 1, "ClassName");
        const char* cn = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        lua_pop(L, 1);

        if (!cn || strcmp(cn, "ModuleScript") != 0) {
            lua_pushnil(L);
            return 1;
        }

        lua_getglobal(L, "require");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, 1);
        if (lua_pcall(L, 1, 1, 0) != 0) {
            lua_pop(L, 1);
            lua_pushnil(L);
            return 1;
        }

        lua_getfenv(L, -1);
        if (lua_istable(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }

        lua_pop(L, 2);
        lua_pushnil(L);
        return 1;
    }

    int getgc(lua_State* L)
    {
        bool include_tables = false;
        if (!lua_isnoneornil(L, 1))
            include_tables = lua_toboolean(L, 1);

        lua_gc(L, LUA_GCCOLLECT, 0);

        luaC_threadbarrier(L);

        lua_gc(L, LUA_GCSTOP, 0);

        lua_newtable(L);
        int result_index = lua_absindex(L, -1);

        getgc_context ctx{ L, include_tables, 0, result_index };
        luaM_visitgco(L, &ctx, getgc_visit);

        lua_gc(L, LUA_GCRESTART, 0);

        return 1;
    }

    int getactors(lua_State* L)
    {
        lua_newtable(L);
        int idx = 1;

        lua_getglobal(L, "game");
        if (lua_isuserdata(L, -1)) {
            lua_getfield(L, -1, "GetDescendants");
            if (lua_isfunction(L, -1)) {
                lua_pushvalue(L, -2);
                if (lua_pcall(L, 1, 1, 0) == 0 && lua_istable(L, -1)) {
                    lua_pushnil(L);
                    while (lua_next(L, -2)) {
                        if (lua_isuserdata(L, -1)) {
                            lua_getfield(L, -1, "ClassName");
                            if (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "Actor") == 0) {
                                lua_pop(L, 1);
                                lua_pushvalue(L, -1);
                                lua_rawseti(L, 1, idx++);
                            }
                            else {
                                lua_pop(L, 1);
                            }
                        }
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
            }
            else {
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        return 1;
    }

    int getregistry(lua_State* L)
    {
        lua_pushvalue(L, LUA_REGISTRYINDEX);
        return 1;
    }

    int getsenv(lua_State* s) {
        luaL_checktype(s, 1, LUA_TUSERDATA);

        std::string type = luaL_typename(s, 1);
        if (type != "Instance")
            luaL_typeerrorL(s, 1, "Instance");

        const auto script = *reinterpret_cast<uintptr_t*>(lua_touserdata(s, 1));

        lua_getfield(s, 1, "ClassName");
        std::string class_name = std::string(lua_tolstring(s, -1, nullptr));
        lua_pop(s, 1);

        if (class_name == "LocalScript" || class_name == "Script") {
            auto node = *reinterpret_cast<uintptr_t*>(script + Offsets::Scripts::weak_thread_node);
            auto weakref = *reinterpret_cast<uintptr_t*>(node + Offsets::Scripts::weak_thread_ref);
            auto liveref = *reinterpret_cast<uintptr_t*>(weakref + Offsets::Scripts::weak_thread_ref_live);
            lua_State* liverefthread = *reinterpret_cast<lua_State**>(liveref + Offsets::Scripts::weak_thread_ref_live_thread);
            if (!liverefthread) {
                lua_pushnil(s);
                return 1;
            }

            if (liverefthread->global->mainthread != s->global->mainthread)
                Roblox::Print(0, "thread is on a different vm");

            luaC_threadbarrier(s);
            luaC_threadbarrier(liverefthread);

            lua_pushvalue(liverefthread, LUA_GLOBALSINDEX);
            lua_xmove(liverefthread, s, 1);
            return 1;
        }
        else if (class_name == "ModuleScript") {
            lua_pushvalue(s, LUA_REGISTRYINDEX);
            lua_pushnil(s);
            while (lua_next(s, -2) != 0) {
                if (lua_type(s, -1) == LUA_TTHREAD) {
                    lua_State* thread = lua_tothread(s, -1);

                    if (thread->userdata && !thread->userdata->Script.expired()) {
                        if ((uintptr_t)(thread->userdata->Script.lock().get()) == script) {
                            luaC_threadbarrier(s);
                            luaC_threadbarrier(thread);

                            lua_pushvalue(thread, LUA_GLOBALSINDEX);
                            lua_xmove(thread, s, 1);
                            lua_pop(s, 2);
                            return 1;
                        }
                    }
                }
                lua_pop(s, 1);
            }
            lua_pop(s, 1);

            lua_pushnil(s);
            return 1;
        }
        else {
            luaL_argerrorL(s, 1, "Invalid script type");
        }
        return 0;
    }

    inline int gettenv(lua_State* L)
    {
        if (!lua_isthread(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        lua_State* thread = lua_tothread(L, 1);
        if (!thread) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(thread, LUA_GLOBALSINDEX);
        lua_xmove(thread, L, 1);
        return 1;
    }

    inline int gethui(lua_State* L)
    {
        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "CoreGui");

        if (lua_pcall(L, 2, 1, 0) == 0) {
            lua_remove(L, -2);
            return 1;
        }

        lua_pop(L, 1);
        lua_getfield(L, -1, "GetService");
        lua_pushvalue(L, -2);
        lua_pushstring(L, "Players");

        if (lua_pcall(L, 2, 1, 0) == 0) {
            lua_getfield(L, -1, "LocalPlayer");
            lua_getfield(L, -1, "PlayerGui");
            lua_remove(L, -2);
            lua_remove(L, -2);
            lua_remove(L, -2);
            return 1;
        }

        lua_newtable(L);
        return 1;
    }

    void SetIdentity(lua_State* l, int lvl)
    {
        const int64_t NewCapabilities = Roblox::GetCapabilities(&lvl) | 0x3FFFFFFFFFFF00i64;
        l->userdata->Identity = lvl;
        l->userdata->Capabilities = NewCapabilities;
        std::uintptr_t identity_struct = Roblox::GetIdentityStruct(*reinterpret_cast<std::uintptr_t*>(Offsets::Identity::IdentityPtr));
        if (!identity_struct) return;
        *reinterpret_cast<std::int32_t*>(identity_struct) = lvl;
        *reinterpret_cast<std::uintptr_t*>(identity_struct + 0x28) = NewCapabilities;
    }

    int setthreadidentity(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TNUMBER);
        int NewIdentity = lua_tointeger(L, 1);
        SetIdentity(L, NewIdentity);
        return 0;
    }

    int getthreadidentity(lua_State* L)
    {
        if (L->userdata)
            lua_pushinteger(L, L->userdata->Identity);
        else
            lua_pushinteger(L, 0);
        return 1;
    }

    static inline bool isNaN(lua_State* L, int idx) {
        if (!lua_isnumber(L, idx)) return false;
        lua_Number n = lua_tonumber(L, idx);
        return n != n;
    }

    static inline bool valuesEqual(lua_State* L, int a, int b) {
        if (isNaN(L, a) && isNaN(L, b)) return true;
        return lua_equal(L, a, b);
    }

    static inline bool tableContainsValue(lua_State* L, int tableIdx, int valueIdx) {
        lua_pushnil(L);
        while (lua_next(L, tableIdx) != 0) {
            if (valuesEqual(L, -1, valueIdx)) {
                lua_pop(L, 2);
                return true;
            }
            lua_pop(L, 1);
        }
        return false;
    }

    static inline bool tableContainsKey(lua_State* L, int tableIdx, int keyIdx) {
        lua_pushnil(L);
        while (lua_next(L, tableIdx) != 0) {
            if (lua_equal(L, -2, keyIdx)) {
                lua_pop(L, 2);
                return true;
            }
            lua_pop(L, 1);
        }
        return false;
    }

    static inline bool tableContainsAllValues(lua_State* L, int sourceTable, int filterTable) {
        lua_pushnil(L);
        while (lua_next(L, filterTable) != 0) {
            int valueIdx = lua_gettop(L);
            if (!tableContainsValue(L, sourceTable, valueIdx)) {
                lua_pop(L, 2);
                return false;
            }
            lua_pop(L, 1);
        }
        return true;
    }

    static inline bool tableContainsAllKeys(lua_State* L, int sourceTable, int filterTable) {
        lua_pushnil(L);
        while (lua_next(L, filterTable) != 0) {
            int keyIdx = lua_gettop(L) - 1;
            if (!tableContainsKey(L, sourceTable, keyIdx)) {
                lua_pop(L, 2);
                return false;
            }
            lua_pop(L, 1);
        }
        return true;
    }

    static inline bool tableContainsAllKeyValuePairs(lua_State* L, int sourceTable, int filterTable) {
        lua_pushnil(L);
        while (lua_next(L, filterTable) != 0) {
            lua_pushvalue(L, -2);
            lua_gettable(L, sourceTable);

            if (!valuesEqual(L, -1, -2)) {
                lua_pop(L, 3);
                return false;
            }
            lua_pop(L, 2);
        }
        return true;
    }

    static inline const char* getOptionalStringField(lua_State* L, int tableIdx, const char* fieldA, const char* fieldB = nullptr) {
        const char* value = nullptr;
        lua_getfield(L, tableIdx, fieldA);
        if (lua_isstring(L, -1)) {
            value = lua_tostring(L, -1);
            lua_pop(L, 1);
            return value;
        }
        lua_pop(L, 1);
        if (fieldB) {
            lua_getfield(L, tableIdx, fieldB);
            if (lua_isstring(L, -1)) {
                value = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
        return value;
    }

    static inline const char* getOptionalStringField3(lua_State* L, int tableIdx, const char* fieldA, const char* fieldB, const char* fieldC) {
        const char* value = getOptionalStringField(L, tableIdx, fieldA, fieldB);
        if (value) return value;
        if (fieldC) return getOptionalStringField(L, tableIdx, fieldC, nullptr);
        return nullptr;
    }

    static inline bool getOptionalBooleanField(lua_State* L, int tableIdx, const char* fieldA, const char* fieldB, bool defaultValue) {
        lua_getfield(L, tableIdx, fieldA);
        if (lua_isboolean(L, -1)) {
            bool value = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return value;
        }
        lua_pop(L, 1);
        if (fieldB) {
            lua_getfield(L, tableIdx, fieldB);
            if (lua_isboolean(L, -1)) {
                bool value = lua_toboolean(L, -1);
                lua_pop(L, 1);
                return value;
            }
            lua_pop(L, 1);
        }
        return defaultValue;
    }

    static inline bool getOptionalBooleanField3(lua_State* L, int tableIdx, const char* fieldA, const char* fieldB, const char* fieldC, bool defaultValue) {
        lua_getfield(L, tableIdx, fieldA);
        if (lua_isboolean(L, -1)) {
            bool value = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return value;
        }
        lua_pop(L, 1);
        if (fieldB) {
            lua_getfield(L, tableIdx, fieldB);
            if (lua_isboolean(L, -1)) {
                bool value = lua_toboolean(L, -1);
                lua_pop(L, 1);
                return value;
            }
            lua_pop(L, 1);
        }
        if (fieldC) {
            lua_getfield(L, tableIdx, fieldC);
            if (lua_isboolean(L, -1)) {
                bool value = lua_toboolean(L, -1);
                lua_pop(L, 1);
                return value;
            }
            lua_pop(L, 1);
        }
        return defaultValue;
    }

    static inline int getOptionalTableFieldRef(lua_State* L, int tableIdx, const char* fieldA, const char* fieldB = nullptr) {
        int ref = LUA_NOREF;
        lua_getfield(L, tableIdx, fieldA);
        if (lua_istable(L, -1)) {
            ref = lua_ref(L, -1);
            lua_pop(L, 1);
            return ref;
        }
        lua_pop(L, 1);
        if (fieldB) {
            lua_getfield(L, tableIdx, fieldB);
            if (lua_istable(L, -1)) {
                ref = lua_ref(L, -1);
            }
            lua_pop(L, 1);
        }
        return ref;
    }

    static inline int getOptionalTableFieldRef3(lua_State* L, int tableIdx, const char* fieldA, const char* fieldB, const char* fieldC = nullptr) {
        int ref = getOptionalTableFieldRef(L, tableIdx, fieldA, fieldB);
        if (ref != LUA_NOREF) return ref;
        if (fieldC) return getOptionalTableFieldRef(L, tableIdx, fieldC, nullptr);
        return LUA_NOREF;
    }

    static inline bool functionNameMatches(lua_State* L, int functionIdx, Closure* cl, const char* expectedName) {
        if (!expectedName) return true;

        const char* rawName = nullptr;
        if (cl->isC) {
            if (cl->c.debugname) rawName = cl->c.debugname;
        }
        else if (cl->l.p && cl->l.p->debugname) {
            rawName = getstr(cl->l.p->debugname);
        }

        if (rawName) {
            if (strcmp(rawName, expectedName) == 0) return true;
            if (strstr(rawName, expectedName) != nullptr) return true;
        }

        int top = lua_gettop(L);

        lua_getglobal(L, "debug");
        if (!lua_istable(L, -1)) {
            lua_settop(L, top);
            return false;
        }

        lua_getfield(L, -1, "info");
        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, functionIdx);
            lua_pushstring(L, "n");
            if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
                if (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), expectedName) == 0) {
                    lua_settop(L, top);
                    return true;
                }
                lua_pop(L, 1);
            }
            else {
                lua_pop(L, 1);
            }
        }
        else {
            lua_pop(L, 1);
        }

        lua_getfield(L, -1, "getinfo");
        if (lua_isfunction(L, -1)) {
            lua_pushvalue(L, functionIdx);
            if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "name");
                    bool matched = lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), expectedName) == 0;
                    lua_pop(L, 2);
                    if (matched) {
                        lua_settop(L, top);
                        return true;
                    }
                }
                else {
                    lua_pop(L, 1);
                }
            }
            else {
                lua_pop(L, 1);
            }
        }
        else {
            lua_pop(L, 1);
        }

        lua_settop(L, top);
        return false;
    }

    inline int filtergc(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TTABLE);

        const char* filterType = lua_tostring(L, 1);
        bool returnOne = luaL_optboolean(L, 3, false);

        bool isFunction = strcmp(filterType, "function") == 0;
        bool isTable = strcmp(filterType, "table") == 0;

        if (!isFunction && !isTable) {
            luaL_argerrorL(L, 1, "expected 'function' or 'table'");
            return 0;
        }

        int optionsTable = 2;

        const char* filterName = nullptr;
        bool ignoreExecutor = true;
        const char* filterHash = nullptr;
        int constantsRef = LUA_NOREF;
        int upvaluesRef = LUA_NOREF;
        int keysRef = LUA_NOREF;
        int valuesRef = LUA_NOREF;
        int kvpRef = LUA_NOREF;
        int metatableRef = LUA_NOREF;

        if (isFunction) {
            filterName = getOptionalStringField3(L, optionsTable, "Name", "name", "NAME");
            ignoreExecutor = getOptionalBooleanField3(L, optionsTable, "IgnoreExecutor", "ignoreexecutor", "ignoreExecutor", true);
            filterHash = getOptionalStringField3(L, optionsTable, "Hash", "hash", "HASH");
            constantsRef = getOptionalTableFieldRef3(L, optionsTable, "Constants", "constants", "CONSTANTS");
            upvaluesRef = getOptionalTableFieldRef3(L, optionsTable, "Upvalues", "upvalues", "UPVALUES");
        }
        else {
            keysRef = getOptionalTableFieldRef3(L, optionsTable, "Keys", "keys", "KEYS");
            valuesRef = getOptionalTableFieldRef3(L, optionsTable, "Values", "values", "VALUES");
            kvpRef = getOptionalTableFieldRef3(L, optionsTable, "KeyValuePairs", "keyvaluepairs", "keyValuePairs");
            metatableRef = getOptionalTableFieldRef3(L, optionsTable, "Metatable", "metatable", "METATABLE");
        }

        lua_newtable(L);
        int gcTableIdx = lua_gettop(L);
        {
            getgc_context gcctx{ L, isTable, 0, gcTableIdx };
            lua_gc(L, LUA_GCCOLLECT, 0);
            luaC_threadbarrier(L);
            lua_gc(L, LUA_GCSTOP, 0);
            luaM_visitgco(L, &gcctx, getgc_visit);
            lua_gc(L, LUA_GCRESTART, 0);
        }

        lua_newtable(L);
        int resultIdx = lua_gettop(L);
        int resultCount = 0;

        lua_pushnil(L);
        while (lua_next(L, gcTableIdx) != 0) {
            int objIdx = lua_gettop(L);
            bool matches = true;

            if (isFunction && lua_isfunction(L, objIdx)) {
                Closure* cl = clvalue(luaA_toobject(L, objIdx));

                if (ignoreExecutor && matches) {
                    if (Closures::isexecutorclosure_check(L, cl))
                        matches = false;
                }

                if (filterName && matches) {
                    if (!functionNameMatches(L, objIdx, cl, filterName)) {
                        matches = false;
                    }
                }

                if (filterHash && matches && !cl->isC) {
                    lua_getglobal(L, "getfunctionhash");
                    if (lua_isfunction(L, -1)) {
                        lua_pushvalue(L, objIdx);
                        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
                            if (!lua_isstring(L, -1) || strcmp(lua_tostring(L, -1), filterHash) != 0) {
                                matches = false;
                            }
                            lua_pop(L, 1);
                        }
                        else {
                            lua_pop(L, 1);
                            matches = false;
                        }
                    }
                    else {
                        lua_pop(L, 1);
                        matches = false;
                    }
                }

                if (constantsRef != LUA_NOREF && matches && !cl->isC) {
                    lua_getglobal(L, "debug");
                    if (lua_istable(L, -1)) {
                        lua_getfield(L, -1, "getconstants");
                        if (lua_isfunction(L, -1)) {
                            lua_pushvalue(L, objIdx);
                            if (lua_pcall(L, 1, 1, 0) != LUA_OK || !lua_istable(L, -1)) {
                                matches = false;
                                if (!lua_isnoneornil(L, -1)) lua_pop(L, 1);
                            }
                            else {
                                int constsIdx = lua_gettop(L);

                                lua_getref(L, constantsRef);
                                int filterConstsIdx = lua_gettop(L);

                                if (!tableContainsAllValues(L, constsIdx, filterConstsIdx)) {
                                    matches = false;
                                }
                                lua_pop(L, 2);
                            }
                        }
                        else {
                            lua_pop(L, 1);
                            matches = false;
                        }
                        lua_pop(L, 1);
                    }
                    else {
                        lua_pop(L, 1);
                        matches = false;
                    }
                }

                if (upvaluesRef != LUA_NOREF && matches && !cl->isC) {
                    lua_getglobal(L, "debug");
                    if (lua_istable(L, -1)) {
                        lua_getfield(L, -1, "getupvalues");
                        if (lua_isfunction(L, -1)) {
                            lua_pushvalue(L, objIdx);
                            if (lua_pcall(L, 1, 1, 0) != LUA_OK || !lua_istable(L, -1)) {
                                matches = false;
                                if (!lua_isnoneornil(L, -1)) lua_pop(L, 1);
                            }
                            else {
                                int upvalsIdx = lua_gettop(L);

                                lua_getref(L, upvaluesRef);
                                int filterUpvalsIdx = lua_gettop(L);

                                if (!tableContainsAllValues(L, upvalsIdx, filterUpvalsIdx)) {
                                    matches = false;
                                }
                                lua_pop(L, 2);
                            }
                        }
                        else {
                            lua_pop(L, 1);
                            matches = false;
                        }
                        lua_pop(L, 1);
                    }
                    else {
                        lua_pop(L, 1);
                        matches = false;
                    }
                }

                if (cl->isC && (filterHash || constantsRef != LUA_NOREF || upvaluesRef != LUA_NOREF)) {
                    matches = false;
                }
            }
            else if (isTable && lua_istable(L, objIdx)) {
                if (metatableRef != LUA_NOREF && matches) {
                    lua_getmetatable(L, objIdx);
                    if (lua_isnil(L, -1)) {
                        matches = false;
                    }
                    else {
                        lua_getref(L, metatableRef);
                        if (!lua_equal(L, -1, -2)) {
                            matches = false;
                        }
                        lua_pop(L, 1);
                    }
                    lua_pop(L, 1);
                }

                if (valuesRef != LUA_NOREF && matches) {
                    lua_getref(L, valuesRef);
                    int filterIdx = lua_gettop(L);

                    if (!tableContainsAllValues(L, objIdx, filterIdx)) {
                        matches = false;
                    }
                    lua_pop(L, 1);
                }

                if (keysRef != LUA_NOREF && matches) {
                    lua_getref(L, keysRef);
                    int filterIdx = lua_gettop(L);

                    if (!tableContainsAllKeys(L, objIdx, filterIdx)) {
                        matches = false;
                    }
                    lua_pop(L, 1);
                }

                if (kvpRef != LUA_NOREF && matches) {
                    lua_getref(L, kvpRef);
                    int filterIdx = lua_gettop(L);

                    if (!tableContainsAllKeyValuePairs(L, objIdx, filterIdx)) {
                        matches = false;
                    }
                    lua_pop(L, 1);
                }
            }
            else {
                matches = false;
            }

            if (matches) {
                if (returnOne) {
                    if (constantsRef != LUA_NOREF) lua_unref(L, constantsRef);
                    if (upvaluesRef != LUA_NOREF) lua_unref(L, upvaluesRef);
                    if (keysRef != LUA_NOREF) lua_unref(L, keysRef);
                    if (valuesRef != LUA_NOREF) lua_unref(L, valuesRef);
                    if (kvpRef != LUA_NOREF) lua_unref(L, kvpRef);
                    if (metatableRef != LUA_NOREF) lua_unref(L, metatableRef);
                    return 1;
                }

                lua_pushvalue(L, objIdx);
                lua_rawseti(L, resultIdx, ++resultCount);
            }

            lua_pop(L, 1);
        }

        if (constantsRef != LUA_NOREF) lua_unref(L, constantsRef);
        if (upvaluesRef != LUA_NOREF) lua_unref(L, upvaluesRef);
        if (keysRef != LUA_NOREF) lua_unref(L, keysRef);
        if (valuesRef != LUA_NOREF) lua_unref(L, valuesRef);
        if (kvpRef != LUA_NOREF) lua_unref(L, kvpRef);
        if (metatableRef != LUA_NOREF) lua_unref(L, metatableRef);

        if (returnOne) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, resultIdx);
        return 1;
    }

    inline int getactorthreads(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        if (strcmp(luaL_typename(L, 1), "Instance") != 0)
        {
            lua_newtable(L);
            return 1;
        }

        lua_getfield(L, 1, "ClassName");
        const char* className = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (!className || strcmp(className, "Actor") != 0)
        {
            lua_newtable(L);
            return 1;
        }

        uintptr_t actorPtr = *reinterpret_cast<uintptr_t*>(lua_touserdata(L, 1));

        lua_newtable(L);
        int index = 1;

        std::lock_guard<std::mutex> lock(g_ActorMutex);

        lua_pushvalue(L, LUA_REGISTRYINDEX);
        lua_pushnil(L);

        while (lua_next(L, -2))
        {
            if (lua_isthread(L, -1))
            {
                lua_State* thread = lua_tothread(L, -1);

                if (thread && thread->userdata)
                {
                    lua_getglobal(thread, "script");
                    if (lua_isuserdata(thread, -1))
                    {
                        lua_getfield(thread, -1, "Parent");
                        if (lua_isuserdata(thread, -1))
                        {
                            uintptr_t parentPtr = *reinterpret_cast<uintptr_t*>(lua_touserdata(thread, -1));
                            if (parentPtr == actorPtr)
                            {
                                lua_pushthread(thread);
                                lua_xmove(thread, L, 1);
                                lua_rawseti(L, -5, index++);
                            }
                        }
                        lua_pop(thread, 2);
                    }
                    else
                    {
                        lua_pop(thread, 1);
                    }
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        return 1;
    }

    inline int isparallel(lua_State* L)
    {
        bool isParallel = false;

        if (L->userdata)
        {
            std::lock_guard<std::mutex> lock(g_ActorMutex);
            auto it = g_ParallelStates.find(L);
            if (it != g_ParallelStates.end())
            {
                isParallel = it->second;
            }
        }

        lua_getglobal(L, "script");
        if (lua_isuserdata(L, -1))
        {
            lua_getfield(L, -1, "Parent");
            if (lua_isuserdata(L, -1))
            {
                lua_getfield(L, -1, "ClassName");
                const char* className = lua_tostring(L, -1);
                if (className && strcmp(className, "Actor") == 0)
                {
                    isParallel = true;
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_pushboolean(L, isParallel);
        return 1;
    }

    inline int runonactor(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        if (!lua_isfunction(L, 2) && !lua_isstring(L, 2))
        {
            luaL_typeerror(L, 2, "function or string");
            return 0;
        }

        if (strcmp(luaL_typename(L, 1), "Instance") != 0)
        {
            luaL_error(L, "Argument #1 must be an Instance");
            return 0;
        }

        lua_getfield(L, 1, "ClassName");
        const char* className = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (!className || strcmp(className, "Actor") != 0)
        {
            luaL_error(L, "Argument #1 must be an Actor");
            return 0;
        }

        uintptr_t actorPtr = *reinterpret_cast<uintptr_t*>(lua_touserdata(L, 1));

        lua_State* actorThread = lua_newthread(L);
        int threadRef = lua_ref(L, -1);
        lua_pop(L, 1);

        luaL_sandboxthread(actorThread);
        TaskScheduler::SetThreadCapabilities(actorThread, 8, MaxCapabilities);

        {
            std::lock_guard<std::mutex> lock(g_ActorMutex);
            g_ParallelStates[actorThread] = true;
            g_ActorThreads[actorPtr].push_back(actorThread);
        }

        lua_pushvalue(L, 1);
        lua_xmove(L, actorThread, 1);
        lua_setglobal(actorThread, "script");

        if (lua_isfunction(L, 2))
        {
            lua_pushvalue(L, 2);
            lua_xmove(L, actorThread, 1);

            int nargs = lua_gettop(L) - 2;
            for (int i = 0; i < nargs; i++)
            {
                lua_pushvalue(L, 3 + i);
                lua_xmove(L, actorThread, 1);
            }

            if (lua_pcall(actorThread, nargs, LUA_MULTRET, 0) != LUA_OK)
            {
                const char* error = lua_tostring(actorThread, -1);
                Roblox::Print(3, "Actor execution error: %s", error);
                lua_pop(actorThread, 1);
            }
        }
        else
        {
            size_t len;
            const char* code = lua_tolstring(L, 2, &len);
            std::string source(code, len);

            std::string bytecode = Execution::CompileScript(source);

            if (luau_load(actorThread, "=Actor", bytecode.c_str(), bytecode.length(), 0) == LUA_OK)
            {
                Closure* closure = clvalue(luaA_toobject(actorThread, -1));
                TaskScheduler::SetProtoCapabilities(closure->l.p, &MaxCapabilities);

                if (lua_pcall(actorThread, 0, LUA_MULTRET, 0) != LUA_OK)
                {
                    const char* error = lua_tostring(actorThread, -1);
                    Roblox::Print(3, "Actor execution error: %s", error);
                    lua_pop(actorThread, 1);
                }
            }
            else
            {
                const char* error = lua_tostring(actorThread, -1);
                Roblox::Print(3, "Actor compile error: %s", error);
                lua_pop(actorThread, 1);
            }
        }

        int results = lua_gettop(actorThread);
        if (results > 0)
        {
            lua_xmove(actorThread, L, results);
            return results;
        }

        return 0;
    }

    inline int createcommchannel(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TSTRING);

        const char* channelName = lua_tostring(L, 1);

        std::lock_guard<std::mutex> lock(g_CommMutex);

        if (g_CommChannels.find(channelName) != g_CommChannels.end())
        {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Channel already exists");
            return 2;
        }

        g_CommChannels[channelName] = std::queue<std::string>();

        lua_pushboolean(L, true);
        return 1;
    }

    inline int commchannel_send(lua_State* L)
    {
        const char* channel = lua_tostring(L, lua_upvalueindex(1));
        luaL_checktype(L, 1, LUA_TSTRING);

        size_t len;
        const char* message = lua_tolstring(L, 1, &len);

        std::lock_guard<std::mutex> lock(g_CommMutex);

        auto it = g_CommChannels.find(channel);
        if (it == g_CommChannels.end())
        {
            lua_pushboolean(L, false);
            return 1;
        }

        it->second.push(std::string(message, len));
        lua_pushboolean(L, true);
        return 1;
    }

    inline int commchannel_receive(lua_State* L)
    {
        const char* channel = lua_tostring(L, lua_upvalueindex(1));

        std::lock_guard<std::mutex> lock(g_CommMutex);

        auto it = g_CommChannels.find(channel);
        if (it == g_CommChannels.end() || it->second.empty())
        {
            lua_pushnil(L);
            return 1;
        }

        std::string message = it->second.front();
        it->second.pop();

        lua_pushlstring(L, message.c_str(), message.size());
        return 1;
    }

    inline int commchannel_peek(lua_State* L)
    {
        const char* channel = lua_tostring(L, lua_upvalueindex(1));

        std::lock_guard<std::mutex> lock(g_CommMutex);

        auto it = g_CommChannels.find(channel);
        if (it == g_CommChannels.end() || it->second.empty())
        {
            lua_pushnil(L);
            return 1;
        }

        std::string message = it->second.front();
        lua_pushlstring(L, message.c_str(), message.size());
        return 1;
    }

    inline int commchannel_clear(lua_State* L)
    {
        const char* channel = lua_tostring(L, lua_upvalueindex(1));

        std::lock_guard<std::mutex> lock(g_CommMutex);

        auto it = g_CommChannels.find(channel);
        if (it != g_CommChannels.end())
        {
            while (!it->second.empty())
            {
                it->second.pop();
            }
        }

        return 0;
    }

    inline int commchannel_size(lua_State* L)
    {
        const char* channel = lua_tostring(L, lua_upvalueindex(1));

        std::lock_guard<std::mutex> lock(g_CommMutex);

        auto it = g_CommChannels.find(channel);
        if (it == g_CommChannels.end())
        {
            lua_pushinteger(L, 0);
            return 1;
        }

        lua_pushinteger(L, static_cast<int>(it->second.size()));
        return 1;
    }

    inline int commchannel_destroy(lua_State* L)
    {
        const char* channel = lua_tostring(L, lua_upvalueindex(1));

        std::lock_guard<std::mutex> lock(g_CommMutex);

        g_CommChannels.erase(channel);

        return 0;
    }

    inline int getcommchannel(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TSTRING);

        const char* channelName = lua_tostring(L, 1);

        {
            std::lock_guard<std::mutex> lock(g_CommMutex);
            if (g_CommChannels.find(channelName) == g_CommChannels.end())
            {
                lua_pushnil(L);
                lua_pushstring(L, "Channel does not exist");
                return 2;
            }
        }

        lua_newtable(L);

        lua_pushstring(L, channelName);
        lua_pushcclosure(L, commchannel_send, nullptr, 1);
        lua_setfield(L, -2, "Send");

        lua_pushstring(L, channelName);
        lua_pushcclosure(L, commchannel_receive, nullptr, 1);
        lua_setfield(L, -2, "Receive");

        lua_pushstring(L, channelName);
        lua_pushcclosure(L, commchannel_peek, nullptr, 1);
        lua_setfield(L, -2, "Peek");

        lua_pushstring(L, channelName);
        lua_pushcclosure(L, commchannel_clear, nullptr, 1);
        lua_setfield(L, -2, "Clear");

        lua_pushstring(L, channelName);
        lua_pushcclosure(L, commchannel_size, nullptr, 1);
        lua_setfield(L, -2, "Size");

        lua_pushstring(L, channelName);
        lua_pushcclosure(L, commchannel_destroy, nullptr, 1);
        lua_setfield(L, -2, "Destroy");

        lua_pushstring(L, channelName);
        lua_setfield(L, -2, "Name");

        return 1;
    }

    inline int comparefunctions(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        luaL_checktype(L, 2, LUA_TFUNCTION);

        Closure* cl1 = clvalue(index2addr(L, 1));
        Closure* cl2 = clvalue(index2addr(L, 2));

        if (!cl1 || !cl2)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        if (cl1 == cl2)
        {
            lua_pushboolean(L, true);
            return 1;
        }

        if (cl1->isC && cl2->isC)
        {
            if (cl1->c.f == cl2->c.f)
            {
                lua_pushboolean(L, true);
                return 1;
            }

            if (cl1->nupvalues != cl2->nupvalues)
            {
                lua_pushboolean(L, false);
                return 1;
            }

            bool upvaluesMatch = true;
            for (int i = 0; i < cl1->nupvalues; i++)
            {
                if (!luaO_rawequalObj(&cl1->c.upvals[i], &cl2->c.upvals[i]))
                {
                    upvaluesMatch = false;
                    break;
                }
            }

            lua_pushboolean(L, upvaluesMatch);
            return 1;
        }

        if (!cl1->isC && !cl2->isC)
        {
            if (cl1->l.p == cl2->l.p)
            {
                lua_pushboolean(L, true);
                return 1;
            }

            Proto* p1 = cl1->l.p;
            Proto* p2 = cl2->l.p;

            if (!p1 || !p2)
            {
                lua_pushboolean(L, false);
                return 1;
            }

            bool protosMatch = (
                p1->sizecode == p2->sizecode &&
                p1->sizek == p2->sizek &&
                p1->sizep == p2->sizep &&
                p1->nups == p2->nups &&
                p1->numparams == p2->numparams &&
                p1->is_vararg == p2->is_vararg
                );

            if (protosMatch && p1->sizecode > 0)
            {
                protosMatch = (memcmp(p1->code, p2->code, p1->sizecode * sizeof(Instruction)) == 0);
            }

            if (protosMatch && p1->sizek > 0)
            {
                for (int i = 0; i < p1->sizek && protosMatch; i++)
                {
                    if (!luaO_rawequalObj(&p1->k[i], &p2->k[i]))
                    {
                        protosMatch = false;
                    }
                }
            }

            lua_pushboolean(L, protosMatch);
            return 1;
        }

        lua_pushboolean(L, false);
        return 1;
    }

    int lz4compress(lua_State* L)
    {
        size_t data_len;
        const char* data = luaL_checklstring(L, 1, &data_len);

        if (data_len == 0) {
            lua_pushlstring(L, "", 0);
            return 1;
        }

        int maxCompressedSize = LZ4_compressBound((int)data_len);
        std::vector<char> compressed(maxCompressedSize);

        int compressedSize = LZ4_compress_default(data, compressed.data(), (int)data_len, maxCompressedSize);

        if (compressedSize <= 0) {
            luaL_error(L, "LZ4 compression failed");
        }

        lua_pushlstring(L, compressed.data(), compressedSize);
        return 1;
    }

    int lz4decompress(lua_State* L)
    {
        size_t compressed_len;
        const char* compressed = luaL_checklstring(L, 1, &compressed_len);
        int expected_size = luaL_checkinteger(L, 2);

        if (compressed_len == 0 || expected_size <= 0) {
            lua_pushlstring(L, "", 0);
            return 1;
        }

        std::vector<char> decompressed(expected_size);

        int decompressedSize = LZ4_decompress_safe(compressed, decompressed.data(), (int)compressed_len, expected_size);

        if (decompressedSize < 0) {
            luaL_error(L, "LZ4 decompression failed");
        }

        lua_pushlstring(L, decompressed.data(), decompressedSize);
        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        Utils::AddFunction(L, "identifyexecutor", identifyexecutor);
        Utils::AddFunction(L, "getexecutorname", getexecutorname);

        Utils::AddFunction(L, "getgenv", getgenv);
        Utils::AddFunction(L, "getrenv", getrenv);
        Utils::AddFunction(L, "getmenv", getmenv);
        Utils::AddFunction(L, "gettenv", gettenv);
        Utils::AddFunction(L, "getsenv", getsenv);
        Utils::AddFunction(L, "getgc", getgc);

        Utils::AddFunction(L, "getactors", getactors);
        Utils::AddFunction(L, "getactorthreads", getactorthreads);
        Utils::AddFunction(L, "isparallel", isparallel);
        Utils::AddFunction(L, "is_parallel", isparallel);
        Utils::AddFunction(L, "runonactor", runonactor);
        Utils::AddFunction(L, "run_on_actor", runonactor);
        Utils::AddFunction(L, "createcommchannel", createcommchannel);
        Utils::AddFunction(L, "create_comm_channel", createcommchannel);
        Utils::AddFunction(L, "getcommchannel", getcommchannel);
        Utils::AddFunction(L, "get_comm_channel", getcommchannel);

        Utils::AddFunction(L, "comparefunctions", comparefunctions);
        Utils::AddFunction(L, "comparefunction", comparefunctions);
        Utils::AddFunction(L, "comparefuncs", comparefunctions);

        Utils::RegisterAliases(L, getthreadidentity, { "getthreadidentity", "getidentity", "getthreadcontext"});
        Utils::RegisterAliases(L, setthreadidentity, { "setthreadidentity", "setidentity", "setthreadcontext" });

        Utils::AddFunction(L, "filtergc", filtergc);

        Utils::AddFunction(L, "getregistry", getregistry);
        Utils::AddFunction(L, "getreg", getregistry);

        Utils::AddFunction(L, "lz4compress", lz4compress);
        Utils::AddFunction(L, "lz4decompress", lz4decompress);

        Utils::AddFunction(L, "gethui", gethui);
    }
}
