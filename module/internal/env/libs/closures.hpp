#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lobject.h>
#include <lfunc.h>
#include <lstring.h>
#include <lapi.h>
#include <ltable.h>
#include <unordered_set>
#include <unordered_map>
#include <string>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/execution/execution.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>
#include <regex>

namespace Closures
{
    static std::unordered_map<Closure*, Closure*> NewCClosureMap;
    static std::unordered_set<Closure*> WrappedClosures;
    static std::unordered_map<Closure*, Closure*> HookedFunctions;
    static std::unordered_map<Closure*, Closure*> OriginalFunctions;

    static int NewCClosureHandler(lua_State* L)
    {
        Closure* wrapper = clvalue(L->ci->func);

        auto it = NewCClosureMap.find(wrapper);
        if (it == NewCClosureMap.end()) {
            luaL_error(L, "newcclosure: unable to find original closure");
            return 0;
        }

        Closure* original = it->second;

        luaC_threadbarrier(L);
        setclvalue(L, L->top, original);
        L->top++;
        lua_insert(L, 1);

        StkId func = L->base;
        L->ci->flags |= LUA_CALLINFO_HANDLE;

        L->baseCcalls++;
        int status = luaD_pcall(L, [](lua_State* L, void* ud) {
            luaD_call(L, (StkId)ud, LUA_MULTRET);
            }, func, savestack(L, func), 0);
        L->baseCcalls--;

        if (status == LUA_ERRRUN) {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (error == "attempt to yield across metamethod/C-call boundary")
                return lua_yield(L, LUA_MULTRET);

            luaL_error(L, "%s", error.c_str());
            return 0;
        }

        expandstacklimit(L, L->top);

        if (status == 0 && (L->status == LUA_YIELD || L->status == LUA_BREAK))
            return -1;

        return lua_gettop(L);
    }

    static int NewCClosureContinuation(lua_State* L, int status)
    {
        if (status != LUA_OK) {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (error == "attempt to yield across metamethod/C-call boundary")
                return lua_yield(L, LUA_MULTRET);

            luaL_error(L, "%s", error.c_str());
        }

        return lua_gettop(L);
    }

    int newcclosure(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);

        Closure* closure = clvalue(index2addr(L, 1));

        if (closure->isC) {
            lua_pushvalue(L, 1);
            return 1;
        }

        for (auto& pair : NewCClosureMap) {
            if (pair.second == closure) {
                luaC_threadbarrier(L);
                setclvalue(L, L->top, pair.first);
                L->top++;
                return 1;
            }
        }

        const char* debugname = lua_isstring(L, 2) ? lua_tostring(L, 2) : "newcclosure";

        lua_pushcclosurek(L, NewCClosureHandler, debugname, 0, NewCClosureContinuation);
        Closure* wrapper = clvalue(index2addr(L, -1));

        NewCClosureMap[wrapper] = closure;
        WrappedClosures.insert(wrapper);

        wrapper->env = closure->env;

        Environment::function_array.push_back(wrapper);

        return 1;
    }

    int newlclosure(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);

        Closure* original = clvalue(index2addr(L, 1));

        if (!original->isC) {
            lua_pushvalue(L, 1);
            return 1;
        }

        lua_newtable(L);
        lua_newtable(L);

        luaC_threadbarrier(L);
        L->top->value.p = original->env;
        L->top->tt = LUA_TTABLE;
        L->top++;
        lua_setfield(L, -2, "__index");

        luaC_threadbarrier(L);
        L->top->value.p = original->env;
        L->top->tt = LUA_TTABLE;
        L->top++;
        lua_setfield(L, -2, "__newindex");

        lua_setreadonly(L, -1, true);
        lua_setmetatable(L, -2);

        lua_pushvalue(L, 1);
        lua_setfield(L, -2, "wrapped_func");

        const char* code = "return wrapped_func(...)";
        std::string bytecode = Execution::CompileScript(code);

        if (luau_load(L, "=newlclosure", bytecode.c_str(), bytecode.size(), 0) != LUA_OK) {
            lua_pop(L, 1);
            luaL_error(L, "Failed to create newlclosure");
            return 0;
        }

        Closure* wrapper = clvalue(index2addr(L, -1));
        if (wrapper && !wrapper->isC && wrapper->l.p) {
            TaskScheduler::SetProtoCapabilities(wrapper->l.p, &MaxCapabilities);
        }

        lua_remove(L, -2);

        return 1;
    }

    int clonefunction(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);

        Closure* original = clvalue(index2addr(L, 1));
        Closure* clone = nullptr;

        luaC_threadbarrier(L);

        if (original->isC) {
            clone = luaF_newCclosure(L, original->nupvalues, original->env);
            if (!clone) {
                luaL_error(L, "Failed to clone C closure");
                return 0;
            }

            clone->c.f = original->c.f;
            clone->c.cont = original->c.cont;
            clone->c.debugname = original->c.debugname;

            for (int i = 0; i < original->nupvalues; i++) {
                setobj2n(L, &clone->c.upvals[i], &original->c.upvals[i]);
            }
        }
        else {
            if (!original->l.p) {
                luaL_error(L, "Invalid Lua closure");
                return 0;
            }

            clone = luaF_newLclosure(L, original->nupvalues, original->env, original->l.p);
            if (!clone) {
                luaL_error(L, "Failed to clone Lua closure");
                return 0;
            }

            for (int i = 0; i < original->nupvalues; i++) {
                clone->l.uprefs[i] = original->l.uprefs[i];
            }
        }

        auto it = NewCClosureMap.find(original);
        if (it != NewCClosureMap.end()) {
            NewCClosureMap[clone] = it->second;
            WrappedClosures.insert(clone);
        }

        luaC_threadbarrier(L);
        setclvalue(L, L->top, clone);
        L->top++;

        return 1;
    }

    int hookfunction(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        luaL_checktype(L, 2, LUA_TFUNCTION);

        Closure* target = clvalue(index2addr(L, 1));
        Closure* hook = clvalue(index2addr(L, 2));

        if (!target || !hook) {
            luaL_error(L, "Invalid closures");
            return 0;
        }

        Closure* backup = nullptr;
        auto it = OriginalFunctions.find(target);
        if (it != OriginalFunctions.end()) {
            backup = it->second;
        }
        else {
            luaC_threadbarrier(L);

            if (target->isC) {
                backup = luaF_newCclosure(L, target->nupvalues, target->env);
                if (!backup) {
                    luaL_error(L, "Failed to clone original function");
                    return 0;
                }

                backup->c.f = target->c.f;
                backup->c.cont = target->c.cont;
                backup->c.debugname = target->c.debugname;

                for (int i = 0; i < target->nupvalues; i++) {
                    setobj2n(L, &backup->c.upvals[i], &target->c.upvals[i]);
                }
            }
            else {
                if (!target->l.p) {
                    luaL_error(L, "Invalid Lua closure");
                    return 0;
                }

                backup = luaF_newLclosure(L, target->nupvalues, target->env, target->l.p);
                if (!backup) {
                    luaL_error(L, "Failed to clone original function");
                    return 0;
                }

                for (int i = 0; i < target->nupvalues; i++) {
                    backup->l.uprefs[i] = target->l.uprefs[i];
                }
            }

            auto ncIt = NewCClosureMap.find(target);
            if (ncIt != NewCClosureMap.end()) {
                NewCClosureMap[backup] = ncIt->second;
                WrappedClosures.insert(backup);
            }

            OriginalFunctions[target] = backup;
        }

        HookedFunctions[target] = hook;

        bool targetIsC = target->isC;
        bool hookIsC = hook->isC;

        bool targetIsNC = NewCClosureMap.find(target) != NewCClosureMap.end();
        bool hookIsNC = NewCClosureMap.find(hook) != NewCClosureMap.end();

        if (targetIsC && hookIsC) {
            if (hook->nupvalues > target->nupvalues) {
                luaL_error(L, "hook has more upvalues than target");
                return 0;
            }

            target->c.f = hook->c.f;
            target->c.cont = hook->c.cont;

            int minUpvals = (hook->nupvalues < target->nupvalues) ? hook->nupvalues : target->nupvalues;
            for (int i = 0; i < minUpvals; i++) {
                setobj2n(L, &target->c.upvals[i], &hook->c.upvals[i]);
            }

            if (hookIsNC) {
                auto hookOriginal = NewCClosureMap[hook];
                NewCClosureMap[target] = hookOriginal;
                WrappedClosures.insert(target);
            }
        }
        else if (!targetIsC && !hookIsC) {
            if (hook->nupvalues > target->nupvalues) {
                luaL_error(L, "hook has more upvalues than target");
                return 0;
            }

            if (!hook->l.p) {
                luaL_error(L, "Invalid hook proto");
                return 0;
            }

            target->l.p = hook->l.p;

            for (int i = 0; i < target->nupvalues; i++) {
                setobj2n(L, &target->l.uprefs[i], luaO_nilobject);
            }

            int minUpvals = (hook->nupvalues < target->nupvalues) ? hook->nupvalues : target->nupvalues;
            for (int i = 0; i < minUpvals; i++) {
                setobj2n(L, &target->l.uprefs[i], &hook->l.uprefs[i]);
            }
        }
        else if (targetIsC && !hookIsC) {
            lua_pushvalue(L, 2);
            int result = newcclosure(L);
            if (result != 1) {
                luaL_error(L, "Failed to wrap Lua hook");
                return 0;
            }

            Closure* wrappedHook = clvalue(index2addr(L, -1));
            lua_pop(L, 1);

            if (!wrappedHook) {
                luaL_error(L, "Failed to wrap Lua hook");
                return 0;
            }

            target->c.f = wrappedHook->c.f;
            target->c.cont = wrappedHook->c.cont;

            int minUpvals = (wrappedHook->nupvalues < target->nupvalues) ? wrappedHook->nupvalues : target->nupvalues;
            for (int i = 0; i < minUpvals; i++) {
                setobj2n(L, &target->c.upvals[i], &wrappedHook->c.upvals[i]);
            }

            HookedFunctions[target] = wrappedHook;
        }
        else if (!targetIsC && hookIsC) {
            lua_pushvalue(L, 2);
            int result = newlclosure(L);
            if (result != 1) {
                luaL_error(L, "Failed to wrap C hook");
                return 0;
            }

            Closure* wrappedHook = clvalue(index2addr(L, -1));
            lua_pop(L, 1);

            if (!wrappedHook || wrappedHook->isC || !wrappedHook->l.p) {
                luaL_error(L, "Failed to wrap C hook");
                return 0;
            }

            target->l.p = wrappedHook->l.p;

            for (int i = 0; i < target->nupvalues; i++) {
                setobj2n(L, &target->l.uprefs[i], luaO_nilobject);
            }

            int minUpvals = (wrappedHook->nupvalues < target->nupvalues) ? wrappedHook->nupvalues : target->nupvalues;
            for (int i = 0; i < minUpvals; i++) {
                setobj2n(L, &target->l.uprefs[i], &wrappedHook->l.uprefs[i]);
            }

            HookedFunctions[target] = wrappedHook;
        }

        luaC_threadbarrier(L);
        setclvalue(L, L->top, backup);
        L->top++;

        return 1;
    }

    int restorefunction(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);

        Closure* target = clvalue(index2addr(L, 1));

        auto it = OriginalFunctions.find(target);
        if (it == OriginalFunctions.end()) {
            lua_pushboolean(L, false);
            return 1;
        }

        Closure* original = it->second;

        if (target->isC && original->isC) {
            target->c.f = original->c.f;
            target->c.cont = original->c.cont;
            target->c.debugname = original->c.debugname;
            target->env = original->env;

            for (int i = 0; i < original->nupvalues && i < target->nupvalues; i++) {
                setobj2n(L, &target->c.upvals[i], &original->c.upvals[i]);
            }
        }
        else if (!target->isC && !original->isC) {
            target->l.p = original->l.p;
            target->env = original->env;

            for (int i = 0; i < original->nupvalues && i < target->nupvalues; i++) {
                setobj2n(L, &target->l.uprefs[i], &original->l.uprefs[i]);
            }
        }

        HookedFunctions.erase(target);
        OriginalFunctions.erase(target);

        lua_pushboolean(L, true);
        return 1;
    }

    static int hookDepth = 0;

    int checkcaller(lua_State* L)
    {
        if (SharedVariables::ExploitThread)
        {
            if (L == SharedVariables::ExploitThread)
            {
                lua_pushboolean(L, true);
                return 1;
            }

            if (L && SharedVariables::ExploitThread->global && L->global == SharedVariables::ExploitThread->global)
            {
                lua_pushboolean(L, true);
                return 1;
            }
        }

        if (L && L->userdata && (L->userdata->Script.expired() || L->userdata->Capabilities == MaxCapabilities))
        {
            lua_pushboolean(L, true);
            return 1;
        }

        lua_Debug ar;

        int startLevel = hookDepth > 0 ? hookDepth + 1 : 1;

        for (int level = startLevel; level <= 10; level++)
        {
            if (!lua_getinfo(L, level, "f", &ar))
                break;

            if (lua_isfunction(L, -1))
            {
                Closure* cl = clvalue(index2addr(L, -1));

                if (cl->isC)
                {
                    for (auto func : Environment::function_array)
                    {
                        if (func == cl || func->c.f == cl->c.f)
                        {
                            lua_pop(L, 1);
                            lua_pushboolean(L, true);
                            return 1;
                        }
                    }
                }
                else
                {
                    if (cl->l.p && cl->l.p->source)
                    {
                        const char* src = getstr(cl->l.p->source);
                        if (src && (strcmp(src, "=loadstring") == 0 || strcmp(src, "@Leafy") == 0 || strcmp(src, "@Vicna") == 0))
                        {
                            lua_pop(L, 1);
                            lua_pushboolean(L, true);
                            return 1;
                        }
                    }
                }

                if (WrappedClosures.find(cl) != WrappedClosures.end())
                {
                    lua_pop(L, 1);
                    lua_pushboolean(L, true);
                    return 1;
                }
            }

            lua_pop(L, 1);
        }

        lua_pushboolean(L, false);
        return 1;
    }

    auto is_executor_closure(lua_State* rl) -> int
    {
        if (lua_type(rl, 1) != LUA_TFUNCTION) { lua_pushboolean(rl, false); return 1; }

        Closure* closure = clvalue(index2addr(rl, 1));
        bool value = false;

        if (lua_isLfunction(rl, 1))
        {
            value = closure->l.p->linedefined;
        }
        else
        {
            for (int i = 0; i < Environment::function_array.size(); i++)
            {
                if (Environment::function_array[i]->c.f == closure->c.f)
                {
                    value = true;
                    break;
                }
            }
        }

        lua_pushboolean(rl, value);
        return 1;
    }

    int iscclosure(lua_State* L)
    {
        if (!lua_isfunction(L, 1))
        {
            lua_pushboolean(L, false);
            return 1;
        }

        lua_pushboolean(L, lua_iscfunction(L, 1));
        return 1;
    }

    int islclosure(lua_State* L)
    {
        if (!lua_isfunction(L, 1))
        {
            lua_pushboolean(L, false);
            return 1;
        }

        lua_pushboolean(L, !lua_iscfunction(L, 1));
        return 1;
    }

    int isnewcclosure(lua_State* L)
    {
        if (lua_type(L, 1) != LUA_TFUNCTION)
        {
            lua_pushboolean(L, false);
            return 1;
        }

        Closure* cl = clvalue(luaA_toobject(L, 1));
        lua_pushboolean(L, NewCClosureMap.find(cl) != NewCClosureMap.end());
        return 1;
    }

    int loadstring(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TSTRING);

        size_t sourceLen = 0;
        const char* source = lua_tolstring(L, 1, &sourceLen);
        const char* chunkname = luaL_optstring(L, 2, "=loadstring");

        std::string bytecode = Execution::CompileScript(std::string(source, sourceLen));

        if (bytecode.empty() || bytecode[0] == 0)
        {
            lua_pushnil(L);
            lua_pushstring(L, bytecode.empty() ? "Failed to compile" : bytecode.c_str() + 1);
            return 2;
        }

        if (luau_load(L, chunkname, bytecode.c_str(), bytecode.size(), 0) != LUA_OK)
        {
            lua_pushnil(L);
            lua_pushvalue(L, -2);
            return 2;
        }

        Closure* func = lua_toclosure(L, -1);
        if (func && !func->isC && func->l.p)
        {
            TaskScheduler::SetProtoCapabilities(func->l.p, &MaxCapabilities);
        }

        lua_setsafeenv(L, LUA_GLOBALSINDEX, false);

        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "newcclosure", newcclosure);
        Utils::AddFunction(L, "newlclosure", newlclosure);
        Utils::AddFunction(L, "isnewcclosure", isnewcclosure);
        Utils::AddFunction(L, "clonefunction", clonefunction);
        Utils::AddFunction(L, "hookfunction", hookfunction);
        Utils::AddFunction(L, "hookfunc", hookfunction);
        Utils::AddFunction(L, "replaceclosure", hookfunction);
        Utils::AddFunction(L, "restorefunction", restorefunction);
        Utils::AddFunction(L, "checkcaller", checkcaller);
        Utils::AddFunction(L, "isexecutorclosure", is_executor_closure);
        Utils::AddFunction(L, "isourclosure", is_executor_closure);
        Utils::AddFunction(L, "checkclosure", is_executor_closure);
        Utils::AddFunction(L, "iscclosure", iscclosure);
        Utils::AddFunction(L, "islclosure", islclosure);
        Utils::AddFunction(L, "loadstring", loadstring);
    }
}
