#pragma once

#include <Windows.h>
#include <lstate.h>
#include <map>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

struct weak_thread_ref_t2 {
    std::atomic< std::int32_t > _refs;
    lua_State* thread;
    std::int32_t thread_ref;
    std::int32_t object_id;
    std::int32_t unk1;
    std::int32_t unk2;

    weak_thread_ref_t2(lua_State* L)
        : thread(L), thread_ref(NULL), object_id(NULL), unk1(NULL), unk2(NULL) {
    };
};
struct live_thread_ref
{
    int __atomic_refs; // 0
    lua_State* th; // 8
    int thread_id; // 16
    int ref_id; // 20
};
struct functionscriptslot_t {
    void* vftable;
    std::uint8_t pad_0[104];
    live_thread_ref* func_ref; // 112

};
struct slot_t {
    int64_t unk_0; // 0
    void* func_0; // 8

    slot_t* next; // 16
    void* __atomic; // 24

    int64_t sig; // 32

    void* func_1; // 40

    functionscriptslot_t* storage; // 48
};

struct SignalConnectionBridge {
    slot_t* islot; // 0
    int64_t __shared_reference_islot; // 8

    int64_t unk0; // 16
    int64_t unk1; // 24
};


namespace Lua_Objects {
    inline std::unordered_map<slot_t*, int64_t> NodeMap;

    class RobloxConnection_t {
    private:
        slot_t* node;
        int64_t sig;
        live_thread_ref* func_ref;
        bool bIsLua;

    public:
        RobloxConnection_t(slot_t* node, bool isLua) : node(node), bIsLua(isLua)
        {
            if (isLua)
            {
                if (node->storage && (node->storage->func_ref != nullptr))
                {
                    func_ref = node->storage->func_ref;
                }
                else
                    func_ref = nullptr;
            }
            else
            {
                func_ref = nullptr;
            }
        }
        bool isLuaConn() { return bIsLua; }
        auto get_function_ref() -> int { return func_ref ? func_ref->ref_id : 0; }
        auto get_thread_ref() -> int { return func_ref ? func_ref->thread_id : 0; }
        auto get_luathread() -> lua_State* { return func_ref ? func_ref->th : nullptr; }
        auto get_node() -> slot_t* { return this->node; }

        auto is_enabled() -> bool { return node->sig != NULL; }

        auto disable() -> void
        {
            if (!NodeMap[node])
            {
                NodeMap[node] = node->sig;
            }
            node->sig = NULL;
        }

        auto enable() -> void
        {
            if (!node->sig)
            {
                node->sig = NodeMap[node];
            }
        }
    };
}

int TRobloxScriptConnection = 0;

static int disable_connection(lua_State* ls)
{
    auto pInfo = reinterpret_cast<Lua_Objects::RobloxConnection_t*>(lua_touserdata(ls, lua_upvalueindex(1)));
    if (!pInfo)
        luaL_error(ls, "Invalid connection userdata in disable_connection");
    pInfo->disable();
    return 0;
}

static int enable_connection(lua_State* ls)
{
    auto pInfo = reinterpret_cast<Lua_Objects::RobloxConnection_t*>(lua_touserdata(ls, lua_upvalueindex(1)));
    if (!pInfo)
        luaL_error(ls, "Invalid connection userdata in enable_connection");
    pInfo->enable();
    return 0;
}

int connection_blank(lua_State* rl) {
    return 0;
}

static int fire_connection(lua_State* ls)
{
    const auto nargs = lua_gettop(ls);

    lua_pushvalue(ls, lua_upvalueindex(1));
    if (lua_isfunction(ls, -1))
    {
        lua_insert(ls, 1);
        lua_call(ls, nargs, 0);
    }

    return 0;
}

static int defer_connection(lua_State* ls)
{
    const auto nargs = lua_gettop(ls);

    lua_getglobal(ls, "task");
    lua_getfield(ls, -1, "defer");
    lua_remove(ls, -2);

    lua_pushvalue(ls, lua_upvalueindex(1));
    if (!lua_isfunction(ls, -1))
        return 0;

    lua_insert(ls, 1);
    lua_insert(ls, 1);

    if (lua_pcall(ls, nargs + 1, 1, 0) != LUA_OK)
        luaL_error(ls, "Error in defer_connection: %s", lua_tostring(ls, -1));

    return 1;
}


static int disconnect_connection(lua_State* ls)
{
    auto pInfo = reinterpret_cast<Lua_Objects::RobloxConnection_t*>(lua_touserdatatagged(ls, lua_upvalueindex(1), 72));
    if (!pInfo)
        luaL_error(ls, "Invalid connection userdata in disconnect_connection");

    if (!pInfo->is_enabled())
        luaL_error(ls, ("Cannot Disconnect a Disabled connection! ( Enable it first before Disconnecting. )"));

    auto pUd = reinterpret_cast<SignalConnectionBridge*>(lua_newuserdatatagged(ls, sizeof(SignalConnectionBridge), TRobloxScriptConnection));
    if (!pUd)
        luaL_error(ls, "Failed to create SignalConnectionBridge userdata");

    pUd->islot = pInfo->get_node();
    pUd->unk0 = 0;
    pUd->unk1 = 0;

    lua_getfield(ls, LUA_REGISTRYINDEX, ("RobloxScriptConnection"));
    if (!lua_istable(ls, -1))
        luaL_error(ls, "RobloxScriptConnection metatable not found in registry");
    lua_setmetatable(ls, -2);

    lua_getfield(ls, -1, ("Disconnect"));
    if (!lua_isfunction(ls, -1))
        luaL_error(ls, "Disconnect function not found in RobloxScriptConnection metatable");

    lua_pushvalue(ls, -2);
    if (lua_pcall(ls, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(ls, -1);
        luaL_error(ls, ("Error while disconnecting connection (%s)"), err);
    }

    return 0;
}

static int mt_newindex(lua_State* ls)
{
    auto pInfo = reinterpret_cast<Lua_Objects::RobloxConnection_t*>(lua_touserdatatagged(ls, 1, 72));
    if (!pInfo)
        luaL_error(ls, "Invalid connection userdata in __newindex");

    const char* key = luaL_checkstring(ls, 2);

    if (strcmp(key, "Enabled") == 0)
    {
        bool enabled = luaL_checkboolean(ls, 3);
        if (enabled)
            pInfo->enable();
        else
            pInfo->disable();
    }

    return 0;
}

static int mt_index(lua_State* ls)
{
    const auto pInfo = reinterpret_cast<Lua_Objects::RobloxConnection_t*>(lua_touserdatatagged(ls, 1, 72));
    const char* key = luaL_checkstring(ls, 2);

    bool is_l_connection = true;

    if (strcmp(key, ("Enabled")) == 0 || strcmp(key, ("Connected")) == 0)
    {
        lua_pushboolean(ls, pInfo->is_enabled());
        return 1;
    }
    else if (strcmp(key, ("Disable")) == 0)
    {
        lua_pushvalue(ls, 1);
        lua_pushcclosure(ls, disable_connection, nullptr, 1);
        return 1;
    }
    else if (strcmp(key, ("Enable")) == 0)
    {
        lua_pushvalue(ls, 1);
        lua_pushcclosure(ls, enable_connection, nullptr, 1);
        return 1;
    }
    else if (strcmp(key, ("LuaConnection")) == 0)
    {
        lua_pushboolean(ls, pInfo->isLuaConn());
        return 1;
    }
    else if (strcmp(key, ("ForeignState")) == 0)
    {
        if (pInfo->isLuaConn() == false) {
            lua_pushboolean(ls, true);
            return 1;
        }

        auto th = pInfo->get_luathread();
        if (!th)
        {
            const auto ref = pInfo->get_thread_ref();
            if (ref) {
                lua_getref(ls, ref);
                if (!lua_isthread(ls, -1))
                {
                    lua_pushboolean(ls, true);
                    return 1;
                }
                else
                {
                    th = lua_tothread(ls, -1);
                    lua_pop(ls, 1);
                }
            }
            else {
                lua_pushboolean(ls, true);
                return 1;
            }
        }

        lua_pushboolean(ls, (th->global != ls->global));
        return 1;
    }
    else if (strcmp(key, ("Function")) == 0)
    {
        if (pInfo->isLuaConn() == false) {
            lua_pushnil(ls);
            return 1;
        }

        auto th = pInfo->get_luathread();
        if (!th)
        {
            const auto ref = pInfo->get_thread_ref();
            if (ref) {
                lua_getref(ls, ref);
                if (!lua_isthread(ls, -1))
                {
                    lua_pushnil(ls);
                    return 1;
                }
                else
                {
                    th = lua_tothread(ls, -1);
                    lua_pop(ls, 1);
                }
            }
            else
            {
                lua_pushnil(ls);
                return 1;
            }
        }

        if (th->global != ls->global)
        {
            lua_pushnil(ls);
            return 1;
        }

        const auto ref = pInfo->get_function_ref();
        if (ref) {
            lua_getref(ls, ref);
            if (!lua_isfunction(ls, -1))
            {
                lua_pushnil(ls);
            }
        }
        else
            lua_pushnil(ls);
        return 1;
    }
    else if (strcmp(key, ("Thread")) == 0)
    {
        if (pInfo->isLuaConn() == false) {
            lua_pushnil(ls);
            return 1;
        }

        auto th = pInfo->get_luathread();
        if (!th)
        {
            const auto ref = pInfo->get_thread_ref();
            if (ref) {
                lua_getref(ls, ref);
                if (!lua_isthread(ls, -1))
                {
                    lua_pushnil(ls);
                    return 1;
                }
                else
                {
                    return 1;
                }
            }
            else {
                lua_pushnil(ls);
                return 1;
            }
        }

        luaC_threadbarrier(ls) setthvalue(ls, ls->top, th) ls->top++;
        return 1;
    }
    else if (strcmp(key, ("Fire")) == 0)
    {
        lua_getfield(ls, 1, ("Function"));
        if (!lua_isfunction(ls, -1)) {
            lua_pushcfunction(ls, connection_blank, nullptr);
        }
        else {
            lua_pushcclosure(ls, fire_connection, nullptr, 1);
        }
        return 1;
    }
    else if (strcmp(key, ("Defer")) == 0)
    {
        lua_getfield(ls, 1, ("Function"));
        lua_pushcclosure(ls, defer_connection, nullptr, 1);
        return 1;
    }
    else if (strcmp(key, ("Disconnect")) == 0)
    {
        lua_pushvalue(ls, 1);
        lua_pushcclosure(ls, disconnect_connection, nullptr, 1);
        return 1;
    }

    return 0;
}

int allcons(lua_State* ls)
{
    luaL_checktype(ls, 1, LUA_TUSERDATA);

    if (strcmp(luaL_typename(ls, 1), ("RBXScriptSignal")) != 0)
        luaL_typeerror(ls, 1, ("RBXScriptSignal"));

    const auto stub = ([](lua_State*) -> int { return 0; });

    static void* funcScrSlotvft = nullptr;
    static void* waitvft = nullptr;
    static void* oncevft = nullptr;
    static void* connectparalellvft = nullptr;

    lua_getfield(ls, 1, "Connect");
    {
        lua_pushvalue(ls, 1);
        lua_pushcfunction(ls, stub, nullptr);
    }
    if (lua_pcall(ls, 2, 1, 0) != 0)
        luaL_error(ls, "Error calling Connect stub: %s", lua_tostring(ls, -1));

    auto sigconbr = reinterpret_cast<SignalConnectionBridge*>(lua_touserdata(ls, -1));
    if (!sigconbr)
        luaL_error(ls, "Failed to retrieve connection stub");

    if (!funcScrSlotvft)
        funcScrSlotvft = sigconbr->islot->storage->vftable;

    auto Node = sigconbr->islot->next;

    if (!TRobloxScriptConnection)
        TRobloxScriptConnection = lua_userdatatag(ls, -1);

    lua_getfield(ls, -1, "Disconnect");
    {
        lua_insert(ls, -2);
    }
    if (lua_pcall(ls, 1, 0, 0) != 0)
        luaL_error(ls, "Error calling Disconnect on stub: %s", lua_tostring(ls, -1));

    if (!oncevft) {
        lua_getfield(ls, 1, "Once");
        if (lua_isfunction(ls, -1)) {
            lua_pushvalue(ls, 1);
            lua_pushcfunction(ls, stub, nullptr);
            if (lua_pcall(ls, 2, 1, 0) == LUA_OK) {
                auto sc = reinterpret_cast<SignalConnectionBridge*>(lua_touserdata(ls, -1));
                if (sc && sc->islot && sc->islot->storage)
                    oncevft = sc->islot->storage->vftable;
                lua_getfield(ls, -1, "Disconnect");
                lua_insert(ls, -2);
                lua_pcall(ls, 1, 0, 0);
            } else {
                lua_pop(ls, 1);
            }
        } else {
            lua_pop(ls, 1);
        }
    }

    if (!connectparalellvft) {
        lua_getfield(ls, 1, "ConnectParallel");
        if (lua_isfunction(ls, -1)) {
            lua_pushvalue(ls, 1);
            lua_pushcfunction(ls, stub, nullptr);
            if (lua_pcall(ls, 2, 1, 0) == LUA_OK) {
                auto sc = reinterpret_cast<SignalConnectionBridge*>(lua_touserdata(ls, -1));
                if (sc && sc->islot && sc->islot->storage)
                    connectparalellvft = sc->islot->storage->vftable;
                lua_getfield(ls, -1, "Disconnect");
                lua_insert(ls, -2);
                lua_pcall(ls, 1, 0, 0);
            } else {
                lua_pop(ls, 1);
            }
        } else {
            lua_pop(ls, 1);
        }
    }

    int idx = 1;
    lua_newtable(ls);

    while (Node)
    {
        auto conn = reinterpret_cast<Lua_Objects::RobloxConnection_t*>(
            lua_newuserdatatagged(ls, sizeof(Lua_Objects::RobloxConnection_t), 72)
            );
        if (Node->storage && (Node->storage->vftable == funcScrSlotvft ||
            Node->storage->vftable == waitvft ||
            Node->storage->vftable == oncevft ||
            Node->storage->vftable == connectparalellvft))
        {
            *conn = Lua_Objects::RobloxConnection_t(Node, true);
        }
        else
        {
            *conn = Lua_Objects::RobloxConnection_t(Node, false);
        }

        lua_newtable(ls);
        lua_pushcfunction(ls, mt_index, nullptr);
        lua_setfield(ls, -2, ("__index"));
        lua_pushcfunction(ls, mt_newindex, nullptr);
        lua_setfield(ls, -2, ("__newindex"));
        lua_pushstring(ls, "Event");
        lua_setfield(ls, -2, ("__type"));
        lua_setmetatable(ls, -2);

        lua_rawseti(ls, -2, idx++);
        Node = Node->next;
    }

    return 1;
}

namespace connectionenv::lua_objects {
    inline std::unordered_map<slot_t*, int64_t> NodeMap;

    class RBXConnection_t {
    private:
        slot_t* node;
        int64_t sig;
        live_thread_ref* func_ref;
        bool bIsLua;

    public:
        RBXConnection_t(slot_t* node, bool isLua) : node(node), bIsLua(isLua)
        {
            if (isLua)
            {
                if (node->storage && (node->storage->func_ref != nullptr))
                {
                    func_ref = node->storage->func_ref;
                }
                else
                    func_ref = nullptr;
            }
            else
            {
                func_ref = nullptr;
            }
        }
        bool isLuaConn() { return bIsLua; }
        auto get_function_ref() -> int { return func_ref ? func_ref->ref_id : 0; }
        auto get_thread_ref() -> int { return func_ref ? func_ref->thread_id : 0; }
        auto get_luathread() -> lua_State* { return func_ref ? func_ref->th : nullptr; }
        auto get_node() -> slot_t* { return this->node; }

        auto is_enabled() -> bool { return node->sig != NULL; }

        auto disable() -> void
        {
            if (!NodeMap[node])
            {
                NodeMap[node] = node->sig;
            }
            node->sig = NULL;
        }

        auto enable() -> void
        {
            if (!node->sig)
            {
                node->sig = NodeMap[node];
            }
        }
    };
}

int TRBXScriptConnection = 0;

namespace Interactions
{
    void checkIsA(lua_State* L, const char* idk) {
        lua_getfield(L, 1, "IsA");
        lua_pushvalue(L, 1);
        lua_pushstring(L, idk);
        lua_call(L, 2, 1);
    }

    void checkIsA(lua_State* L, int idx, const char* idk) {
        lua_getfield(L, idx, "IsA");
        lua_pushvalue(L, idx);
        lua_pushstring(L, idk);
        lua_call(L, 2, 1);
    }

    int firetouchinterest(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TUSERDATA);
        luaL_argexpected(L, lua_isboolean(L, 3) || lua_isnumber(L, 3), 3, ("Bool or Number"));

        if (std::string(luaL_typename(L, 1)) != "Instance")
            luaL_typeerror(L, 1, "Instance");

        if (std::string(luaL_typename(L, 2)) != "Instance")
            luaL_typeerror(L, 2, "Instance");

        checkIsA(L, 1, "BasePart");
        const bool bp_1 = lua_toboolean(L, -1);
        lua_pop(L, 1);

        checkIsA(L, 2, "BasePart");
        const bool bp_2 = lua_toboolean(L, -1);
        lua_pop(L, 1);

        luaL_argexpected(L, bp_1, 1, "BasePart");
        luaL_argexpected(L, bp_2, 2, "BasePart");

        const uintptr_t basePart1 = *static_cast<uintptr_t*>(lua_touserdata(L, 1));
        const uintptr_t basePart2 = *static_cast<uintptr_t*>(lua_touserdata(L, 2));

        const uintptr_t Primitive1 = *reinterpret_cast<uintptr_t*>(basePart1 + Offsets::Instance::Primitive);
        const uintptr_t Primitive2 = *reinterpret_cast<uintptr_t*>(basePart2 + Offsets::Instance::Primitive);

        const uintptr_t Overlap1 = *reinterpret_cast<uintptr_t*>(Primitive1 + Offsets::Instance::Overlap);
        const uintptr_t Overlap2 = *reinterpret_cast<uintptr_t*>(Primitive2 + Offsets::Instance::Overlap);

        if (!Primitive1 || !Primitive2 || !Overlap1 || !Overlap2)
            luaL_error(L, ("lmk if this happens it shouldnt."));

        const int toggle = lua_isboolean(L, 3) ? (lua_toboolean(L, 3) ? 0 : 1) : lua_tointeger(L, 3);
        const bool fire = toggle == 0;

        Roblox::FireTouchInterest(Overlap2, Primitive1, Primitive2, fire, 1);
        return 0;
    }

    int fireproximityprompt(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        lua_getglobal(L, "typeof");
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        const bool inst = (strcmp(lua_tolstring(L, -1, 0), "Instance") == 0);
        lua_pop(L, 1);

        if (!inst)
            luaL_argerror(L, 1, ("Expected an Instance"));

        lua_getfield(L, 1, "IsA");
        lua_pushvalue(L, 1);
        lua_pushstring(L, "ProximityPrompt");
        lua_call(L, 2, 1);
        const bool expected = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (!expected)
            luaL_argerror(L, 1, ("Expected a ProximityPrompt"));

        reinterpret_cast<int(__thiscall*)(std::uintptr_t)>(Roblox::FireProximityPrompt)(*reinterpret_cast<std::uintptr_t*>(lua_touserdata(L, 1)));
        return 0;
    }

    int fireclickdetector(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);

        auto FireDistance = luaL_optnumber(L, 2, 0);
        auto EventName = luaL_optstring(L, 3, "MouseClick");

        lua_getglobal(L, "game");
        lua_getfield(L, -1, "GetService");
        lua_insert(L, -2);
        lua_pushstring(L, "Players");
        lua_pcall(L, 2, 1, 0);
        lua_getfield(L, -1, "LocalPlayer");

        if (strcmp(EventName, "MouseClick") == 0)
        {
            Roblox::FireMouseClick(*static_cast<void**>(lua_touserdata(L, 1)), FireDistance, *static_cast<void**>(lua_touserdata(L, -1)));
        }
        else if (strcmp(EventName, "RightMouseClick") == 0)
        {
            Roblox::FireRightMouseClick(*static_cast<void**>(lua_touserdata(L, 1)), FireDistance, *static_cast<void**>(lua_touserdata(L, -1)));
        }
        else if (strcmp(EventName, "MouseHoverEnter") == 0)
        {
            Roblox::FireMouseHover(*static_cast<void**>(lua_touserdata(L, 1)), *static_cast<void**>(lua_touserdata(L, -1)));
        }
        else if (strcmp(EventName, "MouseHoverLeave") == 0)
        {
            Roblox::FireMouseHoverLeave(*static_cast<void**>(lua_touserdata(L, 1)), *static_cast<void**>(lua_touserdata(L, -1)));
        }

        return 0;
    }

    int getconnections(lua_State* ls)
    {
        luaL_checktype(ls, 1, LUA_TUSERDATA);

        if (strcmp(luaL_typename(ls, 1), ("RBXScriptSignal")) != 0)
            luaL_typeerror(ls, 1, ("robloxscriptsignal"));

        const auto stub = ([](lua_State*) -> int { return 0; });

        static void* funcScrSlotvft = nullptr;
        static void* waitvft = nullptr;
        static void* oncevft = nullptr;
        static void* connectparalellvft = nullptr;

        lua_getfield(ls, 1, "Connect");
        {
            lua_pushvalue(ls, 1);
            lua_pushcfunction(ls, stub, nullptr);
        }
        if (lua_pcall(ls, 2, 1, 0) != 0)
            luaL_error(ls, "Error calling Connect stub: %s", lua_tostring(ls, -1));

        auto sigconbr = reinterpret_cast<SignalConnectionBridge*>(lua_touserdata(ls, -1));
        if (!sigconbr)
            luaL_error(ls, "Failed to retrieve connection stub");

        if (!funcScrSlotvft)
            funcScrSlotvft = sigconbr->islot->storage->vftable;

        auto Node = sigconbr->islot->next;

        if (!TRBXScriptConnection)
            TRBXScriptConnection = lua_userdatatag(ls, -1);

        lua_getfield(ls, -1, "Disconnect");
        {
            lua_insert(ls, -2);
        }
        if (lua_pcall(ls, 1, 0, 0) != 0)
            luaL_error(ls, "Error calling Disconnect on stub: %s", lua_tostring(ls, -1));

        if (!oncevft) {
            lua_getfield(ls, 1, "Once");
            if (lua_isfunction(ls, -1)) {
                lua_pushvalue(ls, 1);
                lua_pushcfunction(ls, stub, nullptr);
                if (lua_pcall(ls, 2, 1, 0) == LUA_OK) {
                    auto sc = reinterpret_cast<SignalConnectionBridge*>(lua_touserdata(ls, -1));
                    if (sc && sc->islot && sc->islot->storage)
                        oncevft = sc->islot->storage->vftable;
                    lua_getfield(ls, -1, "Disconnect");
                    lua_insert(ls, -2);
                    lua_pcall(ls, 1, 0, 0);
                } else {
                    lua_pop(ls, 1);
                }
            } else {
                lua_pop(ls, 1);
            }
        }

        if (!connectparalellvft) {
            lua_getfield(ls, 1, "ConnectParallel");
            if (lua_isfunction(ls, -1)) {
                lua_pushvalue(ls, 1);
                lua_pushcfunction(ls, stub, nullptr);
                if (lua_pcall(ls, 2, 1, 0) == LUA_OK) {
                    auto sc = reinterpret_cast<SignalConnectionBridge*>(lua_touserdata(ls, -1));
                    if (sc && sc->islot && sc->islot->storage)
                        connectparalellvft = sc->islot->storage->vftable;
                    lua_getfield(ls, -1, "Disconnect");
                    lua_insert(ls, -2);
                    lua_pcall(ls, 1, 0, 0);
                } else {
                    lua_pop(ls, 1);
                }
            } else {
                lua_pop(ls, 1);
            }
        }

        int idx = 1;
        lua_newtable(ls);

        while (Node)
        {
            auto conn = reinterpret_cast<connectionenv::lua_objects::RBXConnection_t*>(
                lua_newuserdatatagged(ls, sizeof(connectionenv::lua_objects::RBXConnection_t), 72)
                );
            if (Node->storage && (Node->storage->vftable == funcScrSlotvft ||
                Node->storage->vftable == waitvft ||
                Node->storage->vftable == oncevft ||
                Node->storage->vftable == connectparalellvft))
            {
                *conn = connectionenv::lua_objects::RBXConnection_t(Node, true);
            }
            else
            {
                *conn = connectionenv::lua_objects::RBXConnection_t(Node, false);
            }

            lua_newtable(ls);
            lua_pushcfunction(ls, mt_index, nullptr);
            lua_setfield(ls, -2, ("__index"));
            lua_pushcfunction(ls, mt_newindex, nullptr);
            lua_setfield(ls, -2, ("__newindex"));
            lua_pushstring(ls, "Event");
            lua_setfield(ls, -2, ("__type"));
            lua_setmetatable(ls, -2);

            lua_rawseti(ls, -2, idx++);
            Node = Node->next;
        }

        return 1;
    }

    int firesignal(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        if (strcmp(luaL_typename(L, 1), "RBXScriptSignal") != 0)
            luaL_typeerrorL(L, 1, "RBXScriptSignal");

        const int nargs = lua_gettop(L) - 1;

        getconnections(L);
        lua_remove(L, 1);

        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_getfield(L, -1, "Defer");

            for (int i = 1; i <= nargs; ++i)
                lua_pushvalue(L, i);

            if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                luaL_error(L, "firesignal defer error: %s", err);
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        return 0;
    }

    inline int setproximitypromptduration(lua_State* L)
    {
        if (!lua_isuserdata(L, 1) || !lua_isnumber(L, 2)) {
            return 0;
        }

        lua_getfield(L, 1, "ClassName");
        const char* cn = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        lua_pop(L, 1);

        if (!cn || strcmp(cn, "ProximityPrompt") != 0) {
            return 0;
        }

        double duration = lua_tonumber(L, 2);
        lua_pushnumber(L, duration);
        lua_setfield(L, 1, "HoldDuration");

        return 0;
    }

    inline int getproximitypromptduration(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnumber(L, 0);
            return 1;
        }

        lua_getfield(L, 1, "ClassName");
        const char* cn = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        lua_pop(L, 1);

        if (!cn || strcmp(cn, "ProximityPrompt") != 0) {
            lua_pushnumber(L, 0);
            return 1;
        }

        lua_getfield(L, 1, "HoldDuration");
        if (!lua_isnumber(L, -1)) {
            lua_pop(L, 1);
            lua_pushnumber(L, 0);
            return 1;
        }

        return 1;
    }

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "fireproximityprompt", fireproximityprompt);
        Utils::AddFunction(L, "fireclickdetector", fireclickdetector);
        Utils::AddFunction(L, "firetouchinterest", firetouchinterest);

        Utils::AddFunction(L, "getconnections", getconnections);

        Utils::AddFunction(L, "firesignal", firesignal);

        Utils::AddFunction(L, "setproximitypromptduration", setproximitypromptduration);
        Utils::AddFunction(L, "getproximitypromptduration", getproximitypromptduration);
    }
}
