#pragma once

#include <lgc.h>
#include <lfunc.h>
#include <lmem.h>
#include <lstate.h>
#include <lobject.h>
#include <lapi.h>
#include <lstring.h>
#include <cstring>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/execution/execution.hpp>

namespace Debug
{
    inline auto getClosureFromArg(lua_State* L, int idx, bool allowC, bool pop) -> Closure*
    {
        luaL_checkany(L, idx);

        Closure* result = nullptr;

        if (lua_isnumber(L, idx))
        {
            int level = lua_tointeger(L, idx);

            if (level < 1)
            {
                luaL_argerror(L, idx, "level must be positive");
                return nullptr;
            }

            lua_Debug ar;
            if (!lua_getinfo(L, level, "f", &ar))
            {
                luaL_argerror(L, idx, "invalid level");
                return nullptr;
            }

            if (!lua_isfunction(L, -1))
            {
                lua_pop(L, 1);
                luaL_argerror(L, idx, "level does not point to a function");
                return nullptr;
            }

            if (!allowC && lua_iscfunction(L, -1))
            {
                lua_pop(L, 1);
                luaL_argerror(L, idx, "Lua function expected");
                return nullptr;
            }

            result = clvalue(luaA_toobject(L, -1));
            if (pop) lua_pop(L, 1);
        }
        else if (lua_isfunction(L, idx))
        {
            if (!allowC && lua_iscfunction(L, idx))
            {
                luaL_argerror(L, idx, "Lua function expected");
                return nullptr;
            }

            result = clvalue(luaA_toobject(L, idx));
        }
        else
        {
            luaL_typeerror(L, idx, "function or number");
            return nullptr;
        }

        return result;
    }

    inline auto getUpvalueHelper(lua_State* L, Closure* cl, int idx, TValue** val) -> const char*
    {
        if (!cl || !val)
            return nullptr;

        *val = nullptr;

        if (cl->isC)
        {
            if (idx < 1 || idx > cl->nupvalues)
                return nullptr;

            TValue* upval = &cl->c.upvals[idx - 1];
            if (!upval)
                return nullptr;

            *val = upval;
            return "";
        }
        else
        {
            Proto* p = cl->l.p;
            if (!p)
                return nullptr;

            if (idx < 1 || idx > p->nups)
                return nullptr;

            if (idx > cl->nupvalues)
                return nullptr;

            TValue* r = &cl->l.uprefs[idx - 1];
            if (!r)
                return nullptr;

            if (ttisupval(r))
            {
                UpVal* uv = upvalue(r);
                if (uv && uv->v)
                    *val = uv->v;
                else
                    return nullptr;
            }
            else
            {
                *val = r;
            }

            if (p->upvalues && idx <= p->sizeupvalues)
            {
                TString* name = p->upvalues[idx - 1];
                if (name)
                    return getstr(name);
            }

            return "";
        }
    }

    inline auto debug_getconstants(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, false, true);
        if (!cl)
            return 0;

        Proto* p = cl->l.p;
        if (!p)
        {
            lua_newtable(L);
            return 1;
        }

        int sizek = p->sizek;
        if (sizek <= 0)
        {
            lua_newtable(L);
            return 1;
        }

        lua_createtable(L, sizek, 0);

        for (int i = 0; i < sizek; i++)
        {
            TValue* k = &p->k[i];

            if (!k || k->tt == LUA_TNIL || k->tt == LUA_TFUNCTION || k->tt == LUA_TTABLE)
            {
                lua_pushnil(L);
            }
            else
            {
                luaC_threadbarrier(L);
                luaA_pushobject(L, k);
            }

            lua_rawseti(L, -2, i + 1);
        }

        return 1;
    }

    inline auto debug_getconstant(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, false, true);
        if (!cl)
            return 0;

        int idx = luaL_checkinteger(L, 2);

        Proto* p = cl->l.p;
        if (!p || p->sizek <= 0)
        {
            lua_pushnil(L);
            return 1;
        }

        if (idx < 1 || idx > p->sizek)
        {
            luaL_argerror(L, 2, "index out of range");
            return 0;
        }

        TValue* k = &p->k[idx - 1];

        if (!k || k->tt == LUA_TNIL || k->tt == LUA_TTABLE || k->tt == LUA_TFUNCTION)
        {
            lua_pushnil(L);
        }
        else
        {
            luaC_threadbarrier(L);
            luaA_pushobject(L, k);
        }

        return 1;
    }

    inline auto debug_setconstant(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, false, true);
        if (!cl)
            return 0;

        int idx = luaL_checkinteger(L, 2);
        luaL_checkany(L, 3);

        Proto* p = cl->l.p;
        if (!p || p->sizek <= 0)
        {
            luaL_argerror(L, 1, "function has no constants");
            return 0;
        }

        if (idx < 1 || idx > p->sizek)
        {
            luaL_argerror(L, 2, "index out of range");
            return 0;
        }

        TValue* k = &p->k[idx - 1];
        const TValue* newVal = luaA_toobject(L, 3);

        if (!k || !newVal)
            return 0;

        if (k->tt == LUA_TFUNCTION || k->tt == LUA_TTABLE)
            return 0;

        setobj(L, k, newVal);

        return 0;
    }

    inline auto debug_getupvalues(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, true, true);
        if (!cl)
        {
            lua_newtable(L);
            return 1;
        }

        int nups = cl->nupvalues;
        if (nups <= 0)
        {
            lua_newtable(L);
            return 1;
        }

        lua_createtable(L, nups, 0);

        for (int i = 1; i <= nups; i++)
        {
            TValue* val = nullptr;
            const char* name = getUpvalueHelper(L, cl, i, &val);

            if (name && val)
            {
                luaC_threadbarrier(L);
                luaA_pushobject(L, val);
            }
            else
            {
                lua_pushnil(L);
            }

            lua_rawseti(L, -2, i);
        }

        return 1;
    }

    inline auto debug_getupvalue(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, true, true);
        if (!cl)
        {
            lua_pushnil(L);
            return 1;
        }

        int idx = luaL_checkinteger(L, 2);

        if (cl->nupvalues <= 0)
        {
            lua_pushnil(L);
            return 1;
        }

        if (idx < 1 || idx > cl->nupvalues)
        {
            lua_pushnil(L);
            return 1;
        }

        TValue* val = nullptr;
        const char* name = getUpvalueHelper(L, cl, idx, &val);

        if (!name || !val)
        {
            lua_pushnil(L);
            return 1;
        }

        luaC_threadbarrier(L);
        luaA_pushobject(L, val);

        return 1;
    }

    inline auto debug_setupvalue(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, true, true);
        if (!cl)
            return 0;

        int idx = luaL_checkinteger(L, 2);
        luaL_checkany(L, 3);

        if (cl->nupvalues <= 0)
        {
            luaL_argerror(L, 1, "function has no upvalues");
            return 0;
        }

        if (idx < 1 || idx > cl->nupvalues)
        {
            luaL_argerror(L, 2, "index out of range");
            return 0;
        }

        TValue* val = nullptr;
        const char* name = getUpvalueHelper(L, cl, idx, &val);

        if (!name || !val)
        {
            luaL_argerror(L, 2, "invalid upvalue");
            return 0;
        }

        const TValue* newVal = luaA_toobject(L, 3);
        if (!newVal)
            return 0;

        setobj(L, val, newVal);
        luaC_barrier(L, cl, newVal);

        return 0;
    }

    inline static int inactive_proto_error(lua_State* L)
    {
        luaL_error(L, "cannot call inactive proto");
        return 0;
    }

    inline Proto* clone_proto(lua_State* L, Proto* proto) /* sUnc is pmo, wants the proto to not be callable */
    {
        Proto* clone = luaF_newproto(L);

        clone->sizek = proto->sizek;
        clone->k = luaM_newarray(L, proto->sizek, TValue, proto->memcat);
        for (int i = 0; i < proto->sizek; ++i)
            setobj2n(L, &clone->k[i], &proto->k[i]);

        clone->lineinfo = clone->lineinfo;
        clone->locvars = luaM_newarray(L, proto->sizelocvars, LocVar, proto->memcat);
        for (int i = 0; i < proto->sizelocvars; ++i)
        {
            const auto varname = getstr(proto->locvars[i].varname);
            const auto varname_size = strlen(varname);

            clone->locvars[i].varname = luaS_newlstr(L, varname, varname_size);
            clone->locvars[i].endpc = proto->locvars[i].endpc;
            clone->locvars[i].reg = proto->locvars[i].reg;
            clone->locvars[i].startpc = proto->locvars[i].startpc;
        }

        clone->nups = proto->nups;
        clone->sizeupvalues = proto->sizeupvalues;
        clone->sizelineinfo = proto->sizelineinfo;
        clone->linegaplog2 = proto->linegaplog2;
        clone->sizelocvars = proto->sizelocvars;
        clone->linedefined = proto->linedefined;

        if (proto->debugname)
        {
            const auto debugname = getstr(proto->debugname);
            const auto debugname_size = strlen(debugname);

            clone->debugname = luaS_newlstr(L, debugname, debugname_size);
        }

        if (proto->source)
        {
            const auto source = getstr(proto->source);
            const auto source_size = strlen(source);

            clone->source = luaS_newlstr(L, source, source_size);
        }

        clone->numparams = proto->numparams;
        clone->is_vararg = proto->is_vararg;
        clone->maxstacksize = proto->maxstacksize;
        clone->bytecodeid = proto->bytecodeid;

        auto bytecode = Execution::CompileScript(("return"));
        luau_load(L, ("@cloneproto"), bytecode.c_str(), bytecode.size(), 0);

        Closure* cl = clvalue(index2addr(L, -1));

        clone->sizecode = cl->l.p->sizecode;
        clone->code = luaM_newarray(L, clone->sizecode, Instruction, proto->memcat);
        for (size_t i = 0; i < cl->l.p->sizecode; i++) {
            clone->code[i] = cl->l.p->code[i];
        }
        lua_pop(L, 1);
        clone->codeentry = clone->code;
        clone->debuginsn = 0;

        clone->sizep = proto->sizep;
        clone->p = luaM_newarray(L, proto->sizep, Proto*, proto->memcat);
        for (int i = 0; i < proto->sizep; ++i)
        {
            clone->p[i] = clone_proto(L, proto->p[i]);
        }

        return clone;
    }

    inline auto debug_getproto(lua_State* L) -> int
    {
        luaL_checktype(L, 2, LUA_TNUMBER);


        Closure* closure = nullptr;
        int index = lua_tointeger(L, 2);
        bool active = luaL_optboolean(L, 3, false);

        if (lua_isfunction(L, 1))
        {
            closure = clvalue(luaA_toobject(L, 1));
        }
        else if (lua_isnumber(L, 1))
        {
            lua_Debug dbg_info;

            int level = lua_tointeger(L, 1);
            int callstack_size = static_cast<int>(L->ci - L->base_ci);

            if (level <= 0 || callstack_size <= level)
                luaL_argerrorL(L, 1, ("level out of bounds"));
            if (!lua_getinfo(L, level, ("f"), &dbg_info))
                luaL_argerrorL(L, 1, ("level out of bounds"));

            if (!lua_isfunction(L, -1))
                luaL_argerrorL(L, 1, ("level does not point to a function"));
            if (lua_iscfunction(L, -1))
                luaL_argerrorL(L, 1, ("lua function expected"));;

            closure = clvalue(luaA_toobject(L, -1));
            lua_pop(L, 1);
        }

        Proto* p = closure->l.p;

        if (index <= 0 || index > p->sizep)
            luaL_argerrorL(L, 2, ("index out of bounds"));

        Proto* wanted_proto = p->p[index - 1];

        if (!active)
        {
            Proto* cloned_proto = clone_proto(L, wanted_proto);
            Closure* new_closure = luaF_newLclosure(L, closure->nupvalues, L->gt, cloned_proto);

            luaC_checkGC(L);
            luaC_threadbarrier(L);

            L->top->value.gc = reinterpret_cast<GCObject*>(new_closure);
            L->top->tt = LUA_TFUNCTION;
            luaD_checkstack(L, 1);
            L->top++;
        }
        else
        {
            lua_newtable(L);

            struct Context
            {
                lua_State* L;
                Proto* target;
                int count;
            };

            Context ctx = { L, wanted_proto, 0 };

            luaM_visitgco(L, &ctx, [](void* ctxPtr, lua_Page* page, GCObject* gco) -> bool
                {
                    auto ctx = static_cast<Context*>(ctxPtr);

                    if (!ctx || !ctx->L || !gco)
                        return false;

                    if (isdead(ctx->L->global, gco))
                        return false;

                    if (gco->gch.tt != LUA_TFUNCTION)
                        return false;

                    Closure* gcCl = gco2cl(gco);
                    if (!gcCl || gcCl->isC)
                        return false;

                    if (gcCl->l.p != ctx->target)
                        return false;

                    luaC_threadbarrier(ctx->L);

                    setclvalue(ctx->L, ctx->L->top, gcCl);
                    ctx->L->top++;

                    lua_rawseti(ctx->L, -2, ++ctx->count);

                    return false;
                });
        }

        return 1;

    }

    inline auto debug_getprotos(lua_State* L) -> int
    {
        Closure* cl = getClosureFromArg(L, 1, false, true);
        if (!cl)
        {
            lua_newtable(L);
            return 1;
        }

        Proto* p = cl->l.p;
        if (!p || p->sizep <= 0)
        {
            lua_newtable(L);
            return 1;
        }

        lua_createtable(L, p->sizep, 0);

        for (int i = 0; i < p->sizep; i++)
        {
            Proto* childProto = p->p[i];
            if (!childProto)
            {
                lua_pushcclosure(L, inactive_proto_error, nullptr, 0);
                lua_rawseti(L, -2, i + 1);
                continue;
            }

            lua_pushcclosure(L, inactive_proto_error, nullptr, 0);
            lua_rawseti(L, -2, i + 1);
        }

        return 1;
    }

    inline auto debug_getstack(lua_State* L) -> int
    {
        int level = 0;

        if (lua_isnumber(L, 1))
        {
            level = lua_tointeger(L, 1);

            if (level < 1)
            {
                luaL_argerror(L, 1, "level must be positive");
                return 0;
            }
        }
        else if (lua_isfunction(L, 1))
        {
            level = -lua_gettop(L);
        }
        else
        {
            luaL_typeerror(L, 1, "function or number");
            return 0;
        }

        lua_Debug ar;
        if (!lua_getinfo(L, level, "f", &ar))
        {
            luaL_argerror(L, 1, "invalid level");
            return 0;
        }

        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            luaL_argerror(L, 1, "level does not point to a function");
            return 0;
        }

        if (lua_iscfunction(L, -1))
        {
            lua_pop(L, 1);
            luaL_argerror(L, 1, "Lua function expected");
            return 0;
        }

        lua_pop(L, 1);

        int absLevel = level < 0 ? -level : level;
        int callstackSize = static_cast<int>(L->ci - L->base_ci);

        if (absLevel > callstackSize || absLevel < 1)
        {
            luaL_argerror(L, 1, "level out of range");
            return 0;
        }

        CallInfo* ci = L->ci - absLevel;
        if (!ci || ci < L->base_ci)
        {
            luaL_argerror(L, 1, "invalid call frame");
            return 0;
        }

        if (!ci->base || !ci->top)
        {
            luaL_argerror(L, 1, "invalid call frame");
            return 0;
        }

        int stackSize = static_cast<int>(ci->top - ci->base);

        if (lua_isnumber(L, 2))
        {
            int idx = lua_tointeger(L, 2) - 1;

            if (idx < 0 || idx >= stackSize)
            {
                luaL_argerror(L, 2, "index out of range");
                return 0;
            }

            TValue* val = ci->base + idx;
            if (!val)
            {
                lua_pushnil(L);
                return 1;
            }

            luaC_threadbarrier(L);
            luaA_pushobject(L, val);
        }
        else
        {
            lua_createtable(L, stackSize, 0);

            int idx = 1;
            for (TValue* val = ci->base; val < ci->top; val++)
            {
                if (val)
                {
                    luaC_threadbarrier(L);
                    luaA_pushobject(L, val);
                }
                else
                {
                    lua_pushnil(L);
                }
                lua_rawseti(L, -2, idx++);
            }
        }

        return 1;
    }

    inline auto debug_setstack(lua_State* L) -> int
    {
        int level = 0;

        if (lua_isnumber(L, 1))
        {
            level = lua_tointeger(L, 1);

            if (level < 1)
            {
                luaL_argerror(L, 1, "level must be positive");
                return 0;
            }
        }
        else if (lua_isfunction(L, 1))
        {
            level = -lua_gettop(L);
        }
        else
        {
            luaL_typeerror(L, 1, "function or number");
            return 0;
        }

        int idx = luaL_checkinteger(L, 2);
        luaL_checkany(L, 3);

        lua_Debug ar;
        if (!lua_getinfo(L, level, "f", &ar))
        {
            luaL_argerror(L, 1, "invalid level");
            return 0;
        }

        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            luaL_argerror(L, 1, "level does not point to a function");
            return 0;
        }

        if (lua_iscfunction(L, -1))
        {
            lua_pop(L, 1);
            luaL_argerror(L, 1, "Lua function expected");
            return 0;
        }

        lua_pop(L, 1);

        int absLevel = level < 0 ? -level : level;
        int callstackSize = static_cast<int>(L->ci - L->base_ci);

        if (absLevel > callstackSize || absLevel < 1)
        {
            luaL_argerror(L, 1, "level out of range");
            return 0;
        }

        CallInfo* ci = L->ci - absLevel;
        if (!ci || ci < L->base_ci)
        {
            luaL_argerror(L, 1, "invalid call frame");
            return 0;
        }

        if (!ci->base || !ci->top)
        {
            luaL_argerror(L, 1, "invalid call frame");
            return 0;
        }

        int stackIdx = idx - 1;
        int stackSize = static_cast<int>(ci->top - ci->base);

        if (stackIdx < 0 || stackIdx >= stackSize)
        {
            luaL_argerror(L, 2, "index out of range");
            return 0;
        }

        TValue* target = ci->base + stackIdx;
        if (!target)
        {
            luaL_argerror(L, 2, "invalid stack slot");
            return 0;
        }

        const TValue* newVal = luaA_toobject(L, 3);
        if (!newVal)
            return 0;

        setobj(L, target, newVal);

        return 0;
    }

    inline auto debug_getinfo(lua_State* L) -> int
    {
        luaL_checkany(L, 1);

        if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
        {
            luaL_typeerror(L, 1, "function or number");
            return 0;
        }

        int level;
        if (lua_isnumber(L, 1))
        {
            level = lua_tointeger(L, 1);
        }
        else
        {
            level = -lua_gettop(L);
        }

        lua_Debug ar;
        std::memset(&ar, 0, sizeof(ar));

        if (!lua_getinfo(L, level, "sluanf", &ar))
        {
            luaL_argerror(L, 1, "invalid level");
            return 0;
        }

        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            luaL_argerror(L, 1, "level does not point to a function");
            return 0;
        }

        int funcIdx = lua_gettop(L);

        lua_newtable(L);

        lua_pushstring(L, ar.source ? ar.source : "");
        lua_setfield(L, -2, "source");

        lua_pushstring(L, ar.short_src ? ar.short_src : "");
        lua_setfield(L, -2, "short_src");

        lua_pushstring(L, ar.what ? ar.what : "");
        lua_setfield(L, -2, "what");

        lua_pushinteger(L, ar.linedefined);
        lua_setfield(L, -2, "linedefined");

        lua_pushinteger(L, ar.currentline);
        lua_setfield(L, -2, "currentline");

        lua_pushinteger(L, ar.nupvals);
        lua_setfield(L, -2, "nups");

        lua_pushinteger(L, ar.isvararg ? 1 : 0);
        lua_setfield(L, -2, "is_vararg");

        lua_pushinteger(L, ar.nparams);
        lua_setfield(L, -2, "numparams");

        lua_pushstring(L, ar.name ? ar.name : "");
        lua_setfield(L, -2, "name");

        lua_pushvalue(L, funcIdx);
        lua_setfield(L, -2, "func");

        lua_remove(L, funcIdx);

        return 1;
    }

    inline auto debug_setmetatable(lua_State* L) -> int
    {
        luaL_checkany(L, 1);

        if (lua_isnoneornil(L, 2))
        {
            lua_pushnil(L);
            lua_setmetatable(L, 1);
        }
        else
        {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_pushvalue(L, 2);
            lua_setmetatable(L, 1);
        }

        lua_pushvalue(L, 1);
        return 1;
    }

    inline auto debug_getmetatable(lua_State* L) -> int
    {
        luaL_checkany(L, 1);

        if (!lua_getmetatable(L, 1))
        {
            lua_pushnil(L);
        }

        return 1;
    }

    inline auto debug_traceback(lua_State* L) -> int
    {
        lua_State* targetL = L;
        int argStart = 1;

        if (lua_isthread(L, 1))
        {
            targetL = lua_tothread(L, 1);
            argStart = 2;
        }

        const char* msg = luaL_optstring(L, argStart, nullptr);
        int level = luaL_optinteger(L, argStart + 1, 1);

        luaL_traceback(L, targetL, msg, level);

        return 1;
    }

    inline auto RegisterLibrary(lua_State* L) -> void
    {
        lua_getglobal(L, "debug");

        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
        }

        lua_setreadonly(L, -1, false);

        Utils::AddTableFunction(L, "getconstants", debug_getconstants);
        Utils::AddTableFunction(L, "getconstant", debug_getconstant);
        Utils::AddTableFunction(L, "setconstant", debug_setconstant);

        Utils::AddTableFunction(L, "getupvalues", debug_getupvalues);
        Utils::AddTableFunction(L, "getupvalue", debug_getupvalue);
        Utils::AddTableFunction(L, "setupvalue", debug_setupvalue);

        Utils::AddTableFunction(L, "getproto", debug_getproto);
        Utils::AddTableFunction(L, "getprotos", debug_getprotos);

        Utils::AddTableFunction(L, "getstack", debug_getstack);
        Utils::AddTableFunction(L, "setstack", debug_setstack);

        Utils::AddTableFunction(L, "getinfo", debug_getinfo);

        Utils::AddTableFunction(L, "setmetatable", debug_setmetatable);
        Utils::AddTableFunction(L, "getmetatable", debug_getmetatable);
        Utils::AddTableFunction(L, "traceback", debug_traceback);

        lua_setreadonly(L, -1, true);
        lua_setglobal(L, "debug");

        Utils::AddFunction(L, "getinfo", debug_getinfo);
        Utils::AddFunction(L, "getupvalues", debug_getupvalues);
        Utils::AddFunction(L, "getupvalue", debug_getupvalue);
        Utils::AddFunction(L, "setupvalue", debug_setupvalue);
        Utils::AddFunction(L, "getconstants", debug_getconstants);
        Utils::AddFunction(L, "getconstant", debug_getconstant);
        Utils::AddFunction(L, "setconstant", debug_setconstant);
        Utils::AddFunction(L, "getprotos", debug_getprotos);
        Utils::AddFunction(L, "getproto", debug_getproto);
        Utils::AddFunction(L, "getstack", debug_getstack);
        Utils::AddFunction(L, "setstack", debug_setstack);
    }
}