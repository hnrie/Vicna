#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lgc.h>
#include <lualib.h>
#include <string>
#include <sstream>
#include <map>
#include <unordered_set>

#include <internal/utils.hpp>
#include <internal/globals.hpp>
#include <internal/env/libs/filesys.hpp>
#include <luacode.h>
#include <internal/env/helpers/bytecode/bytecode.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>
#include <lmem.h>
#include <lapi.h>
#include <internal/env/yield/yield.hpp>
#include <internal/env/helpers/decompiler/decompile_helper.hpp>

namespace reflection {

    enum reflection_type : uint32_t
    {
        ReflectionType_Void = 0x0,
        ReflectionType_Bool = 0x1,
        ReflectionType_Int = 0x2,
        ReflectionType_Int64 = 0x3,
        ReflectionType_Float = 0x4,
        ReflectionType_Double = 0x5,
        ReflectionType_String = 0x6,
        ReflectionType_ProtectedString = 0x7,
        ReflectionType_Instance = 0x8,
        ReflectionType_Instances = 0x9,
        ReflectionType_Ray = 0xa,
        ReflectionType_Vector2 = 0xb,
        ReflectionType_Vector3 = 0xc,
        ReflectionType_Vector2Int16 = 0xd,
        ReflectionType_Vector3Int16 = 0xe,
        ReflectionType_Rect2d = 0xf,
        ReflectionType_CoordinateFrame = 0x10,
        ReflectionType_Color3 = 0x11,
        ReflectionType_Color3uint8 = 0x12,
        ReflectionType_UDim = 0x13,
        ReflectionType_UDim2 = 0x14,
        ReflectionType_Faces = 0x15,
        ReflectionType_Axes = 0x16,
        ReflectionType_Region3 = 0x17,
        ReflectionType_Region3Int16 = 0x18,
        ReflectionType_CellId = 0x19,
        ReflectionType_GuidData = 0x1a,
        ReflectionType_PhysicalProperties = 0x1b,
        ReflectionType_BrickColor = 0x1c,
        ReflectionType_SystemAddress = 0x1d,
        ReflectionType_BinaryString = 0x1e,
        ReflectionType_Surface = 0x1f,
        ReflectionType_Enum = 0x20,
        ReflectionType_Property = 0x21,
        ReflectionType_Tuple = 0x22,
        ReflectionType_ValueArray = 0x23,
        ReflectionType_ValueTable = 0x24,
        ReflectionType_ValueMap = 0x25,
        ReflectionType_Variant = 0x26,
        ReflectionType_GenericFunction = 0x27,
        ReflectionType_WeakFunctionRef = 0x28,
        ReflectionType_ColorSequence = 0x29,
        ReflectionType_ColorSequenceKeypoint = 0x2a,
        ReflectionType_NumberRange = 0x2b,
        ReflectionType_NumberSequence = 0x2c,
        ReflectionType_NumberSequenceKeypoint = 0x2d,
        ReflectionType_InputObject = 0x2e,
        ReflectionType_Connection = 0x2f,
        ReflectionType_ContentId = 0x30,
        ReflectionType_DescribedBase = 0x31,
        ReflectionType_RefType = 0x32,
        ReflectionType_QFont = 0x33,
        ReflectionType_QDir = 0x34,
        ReflectionType_EventInstance = 0x35,
        ReflectionType_TweenInfo = 0x36,
        ReflectionType_DockWidgetPluginGuiInfo = 0x37,
        ReflectionType_PluginDrag = 0x38,
        ReflectionType_Random = 0x39,
        ReflectionType_PathWaypoint = 0x3a,
        ReflectionType_FloatCurveKey = 0x3b,
        ReflectionType_RotationCurveKey = 0x3c,
        ReflectionType_SharedString = 0x3d,
        ReflectionType_DateTime = 0x3e,
        ReflectionType_RaycastParams = 0x3f,
        ReflectionType_RaycastResult = 0x40,
        ReflectionType_OverlapParams = 0x41,
        ReflectionType_LazyTable = 0x42,
        ReflectionType_DebugTable = 0x43,
        ReflectionType_CatalogSearchParams = 0x44,
        ReflectionType_OptionalCoordinateFrame = 0x45,
        ReflectionType_CSGPropertyData = 0x46,
        ReflectionType_UniqueId = 0x47,
        ReflectionType_Font = 0x48,
        ReflectionType_Blackboard = 0x49,
        ReflectionType_Max = 0x4a
    };

    struct sys_addr
    {
        struct PEERID {
            int peer_id;
        };

        PEERID remote_id;
    };

    struct shared_str {
        char pad_0[0x10];
        std::string content;
    };
};

namespace Script
{
    inline std::map<void*, std::string> ScriptHashes;

    struct ScriptableCacheEntry {
        uintptr_t instance;
        std::string property_name;
        bool is_scriptable;

        ScriptableCacheEntry(uintptr_t inst, const char* prop, bool scriptable)
            : instance(inst), property_name(prop), is_scriptable(scriptable) {
        }
    };

    inline std::vector<ScriptableCacheEntry> s_ScriptableCache;

    inline std::unordered_map<std::string, bool> s_DefaultPropStates;

    inline void add_defualtprop_state(const char* prop_name, bool was_scriptable) {
        std::string key(prop_name);
        if (s_DefaultPropStates.find(key) == s_DefaultPropStates.end()) {
            s_DefaultPropStates[key] = was_scriptable;
        }
    }

    inline bool find_upd_scriptablecache(uintptr_t instance, const char* prop_name, bool new_state) {
        for (auto& entry : s_ScriptableCache) {
            if (entry.instance == instance && entry.property_name == prop_name) {
                entry.is_scriptable = new_state;
                return true;
            }
        }
        return false;
    }

    inline int get_cachedscriptable_prop(uintptr_t instance, const char* prop_name) {
        for (const auto& entry : s_ScriptableCache) {
            if (entry.instance == instance && entry.property_name == prop_name) {
                return entry.is_scriptable ? 1 : 0;
            }
        }
        return -1;
    }

    inline int getcacheddefscriptableprop(const char* prop_name) {
        auto it = s_DefaultPropStates.find(prop_name);
        if (it != s_DefaultPropStates.end()) {
            return it->second ? 1 : 0;
        }
        return -1;
    }

    inline bool IsValidScriptClass(const char* className)
    {
        if (!className) return false;
        return strcmp(className, "LocalScript") == 0 ||
            strcmp(className, "ModuleScript") == 0 ||
            strcmp(className, "Script") == 0;
    }

    inline std::string Blake2Hash(const char* data, size_t len)
    {
        if (!data || len == 0) return std::string(96, '0');

        uint64_t h[8] = {
            0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
            0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
            0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
            0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
        };

        for (size_t i = 0; i < len; i++) {
            uint64_t val = static_cast<unsigned char>(data[i]);
            h[i % 8] ^= val;
            h[i % 8] = (h[i % 8] << 7) | (h[i % 8] >> 57);
            h[(i + 1) % 8] += val;
        }

        for (int round = 0; round < 12; round++) {
            for (int i = 0; i < 8; i++) {
                h[i] ^= h[(i + 1) % 8];
                h[i] = (h[i] << 13) | (h[i] >> 51);
            }
        }

        char output[97];
        for (int i = 0; i < 6; i++) {
            sprintf_s(output + (i * 16), 17, "%016llx", h[i]);
        }
        output[96] = '\0';

        return std::string(output);
    }

    inline uintptr_t GetInstancePropertyByName(const uintptr_t instance, const std::string& targetPropertyName)
    {
        const uintptr_t classDescriptor =
            *reinterpret_cast<uintptr_t*>(instance + 0x18);

        const uintptr_t propertiesStart =
            *reinterpret_cast<uintptr_t*>(classDescriptor + 0x28);

        for (uint32_t i = 0; i < 80; ++i)
        {
            const uintptr_t entry =
                propertiesStart + (i * 0x10);

            const uintptr_t prop =
                *reinterpret_cast<uintptr_t*>(entry);

            if (!prop)
                continue;

            const uintptr_t namePtr =
                *reinterpret_cast<uintptr_t*>(prop + 0x8);

            if (!namePtr)
                continue;

            const char* name =
                reinterpret_cast<const char*>(namePtr);

            if (name && targetPropertyName == name)
                return prop;
        }

        return 0;
    }

    inline int isscriptable(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TSTRING);

        void* instance_ptr = lua_touserdata(L, 1);
        if (!instance_ptr) {
            lua_pushnil(L);
            return 1;
        }
        uintptr_t instance = *reinterpret_cast<uintptr_t*>(instance_ptr);

        const char* property_name_cstr = lua_tostring(L, 2);
        if (!property_name_cstr) {
            lua_pushnil(L);
            return 1;
        }
        std::string property_name(property_name_cstr);

        uintptr_t property = GetInstancePropertyByName(instance, property_name);
        if (!property) {
            lua_pushnil(L);
            return 1;
        }

        int cached = get_cachedscriptable_prop(instance, property_name.c_str());
        if (cached != -1) {
            lua_pushboolean(L, cached);
            return 1;
        }

        int cached_default = getcacheddefscriptableprop(property_name.c_str());
        if (cached_default != -1) {
            lua_pushboolean(L, cached_default);
            return 1;
        }

        uint64_t bit_flags = *reinterpret_cast<uint64_t*>(property + Offsets::Instance::PropertyDescriptorBitFlags);
        bool is_scriptable = (bit_flags & Offsets::Instance::ScriptableMask) != 0;
        lua_pushboolean(L, is_scriptable);
        return 1;
    }

    inline int setscriptable(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TBOOLEAN);

        void* instance_ptr = lua_touserdata(L, 1);
        if (!instance_ptr) {
            lua_pushnil(L);
            return 1;
        }
        uintptr_t instance = *reinterpret_cast<uintptr_t*>(instance_ptr);

        const char* property_name_cstr = lua_tostring(L, 2);
        if (!property_name_cstr) {
            lua_pushnil(L);
            return 1;
        }
        std::string property_name(property_name_cstr);

        bool new_scriptable_state = lua_toboolean(L, 3);

        uintptr_t property = GetInstancePropertyByName(instance, property_name);
        if (!property) {
            Roblox::Print(3, "Property not found");
            lua_pushnil(L);
            return 1;
        }

        if (!find_upd_scriptablecache(instance, property_name.c_str(), new_scriptable_state)) {
            s_ScriptableCache.emplace_back(instance, property_name.c_str(), new_scriptable_state);
        }

        uint64_t* bit_flags_ptr = reinterpret_cast<uint64_t*>(property + Offsets::Instance::PropertyDescriptorBitFlags);
        uint64_t current_flags = *bit_flags_ptr;
        bool was_scriptable = (current_flags & Offsets::Instance::ScriptableMask) != 0;

        add_defualtprop_state(property_name.c_str(), was_scriptable);

        if (new_scriptable_state) {
            *bit_flags_ptr = current_flags | Offsets::Instance::ScriptableMask;
        }
        else {
            *bit_flags_ptr = current_flags & ~Offsets::Instance::ScriptableMask;
        }

        lua_pushboolean(L, was_scriptable);
        return 1;
    }

    inline int getscripthash(lua_State* L)
    {
        if (!L || !lua_isuserdata(L, 1)) {
            lua_pushstring(L, std::string(96, '0').c_str());
            return 1;
        }

        int top = lua_gettop(L);

        lua_getfield(L, 1, "ClassName");
        if (!lua_isstring(L, -1)) {
            lua_settop(L, top);
            lua_pushstring(L, std::string(96, '0').c_str());
            return 1;
        }

        const char* cn = lua_tostring(L, -1);
        if (!IsValidScriptClass(cn)) {
            lua_settop(L, top);
            lua_pushstring(L, std::string(96, '0').c_str());
            return 1;
        }

        lua_settop(L, top);

        std::string hashData;

        lua_getfield(L, 1, "Name");
        if (lua_isstring(L, -1)) {
            const char* name = lua_tostring(L, -1);
            if (name) hashData += name;
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "Source");
        if (lua_isstring(L, -1)) {
            size_t len;
            const char* source = lua_tolstring(L, -1, &len);
            if (source && len > 0) {
                hashData.append(source, len);
            }
        }
        lua_pop(L, 1);

        void* ptr = *(void**)lua_touserdata(L, 1);
        if (ptr) {
            char ptrStr[32];
            sprintf_s(ptrStr, sizeof(ptrStr), "%p", ptr);
            hashData += ptrStr;
        }

        std::string hash = Blake2Hash(hashData.c_str(), hashData.length());

        if (ptr) {
            ScriptHashes[ptr] = hash;
        }

        lua_pushstring(L, hash.c_str());
        return 1;
    }

    inline int getscriptbytecode(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        std::string ud_t = luaL_typename(L, 1);
        if (ud_t != "Instance") {
            lua_pushnil(L);
            return 1;
        }

        auto script = *reinterpret_cast<std::uintptr_t*>(lua_touserdata(L, 1));
        if (!script) {
            lua_pushnil(L);
            return 1;
        }

        const char* className = *reinterpret_cast<const char**>(
            *reinterpret_cast<uintptr_t*>(script + Offsets::Instance::ClassDescriptor) +
            Offsets::Instance::ClassName
            );

        if (!className) {
            lua_pushnil(L);
            return 1;
        }

        std::uintptr_t protected_string = 0;

        if (strcmp(className, "LocalScript") == 0 || strcmp(className, "Script") == 0) {
            protected_string = *reinterpret_cast<std::uintptr_t*>(script + Offsets::Scripts::LocalScriptByteCode);
        }
        else if (strcmp(className, "ModuleScript") == 0) {
            protected_string = *reinterpret_cast<std::uintptr_t*>(script + Offsets::Scripts::ModuleScriptByteCode);
        }
        else {
            lua_pushnil(L);
            return 1;
        }

        if (!protected_string || IsBadReadPtr(reinterpret_cast<const void*>(protected_string), 0x30)) {
            lua_pushnil(L);
            return 1;
        }

        char* data_ptr = *reinterpret_cast<char**>(protected_string + 0x10);
        size_t data_size = *reinterpret_cast<size_t*>(protected_string + 0x20);

        if (data_size <= 15) {
            data_ptr = reinterpret_cast<char*>(protected_string + 0x10);
        }

        if (!data_ptr || data_size == 0 || data_size > 0x10000000) {
            lua_pushnil(L);
            return 1;
        }

        if (IsBadReadPtr(data_ptr, data_size)) {
            lua_pushnil(L);
            return 1;
        }

        std::string compressed_bytecode(data_ptr, data_size);

        if (compressed_bytecode.empty() || compressed_bytecode.size() < 8) {
            lua_pushnil(L);
            return 1;
        }

        std::string decompressed = Bytecode::decompress_bytecode(compressed_bytecode);

        if (decompressed.empty()) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushlstring(L, decompressed.data(), decompressed.size());
        return 1;
    }

    inline int decompile(lua_State* L)
    {
        if (lua_type(L, 1) != LUA_TUSERDATA) {
            lua_pushstring(L, "-- Decompile failed: invalid argument (expected script instance)");
            return 1;
        }

        std::string bytecode = decompile::GetScriptBytecodeStr(L, 1);
        if (bytecode.empty()) {
            lua_pushstring(L, "-- Decompile failed: could not retrieve bytecode");
            return 1;
        }

        return Yielding::YieldExecution(L, [bytecode]() -> Yielded {
            std::string result = decompile::HttpPost("api.plusgiant5.com", INTERNET_DEFAULT_HTTP_PORT, "/konstant/decompile", bytecode.data(), bytecode.size());

            return [result](lua_State* L) -> int {
                if (result.empty()) {
                    lua_pushstring(L, "-- decompile failed");
                }
                else {
                    lua_pushlstring(L, result.data(), result.size());
                }
                return 1;
                };
            });
    }

    inline uintptr_t GetInstancePropertyByName2(const uintptr_t instance, const std::string& targetPropertyName)
    {
        const uintptr_t classDescriptor =
            *reinterpret_cast<uintptr_t*>(instance + 0x18);

        const uintptr_t propertiesStart =
            *reinterpret_cast<uintptr_t*>(classDescriptor + 0x28);

        for (uint32_t i = 0; i < 90; ++i)
        {
            const uintptr_t entry =
                propertiesStart + (i * 0x10);

            const uintptr_t prop =
                *reinterpret_cast<uintptr_t*>(entry);

            if (!prop)
                continue;

            const uintptr_t namePtr =
                *reinterpret_cast<uintptr_t*>(prop + 0x8);

            if (!namePtr)
                continue;

            const char* name =
                reinterpret_cast<const char*>(namePtr);

            if (name && targetPropertyName == name)
                return prop;
        }

        return 0;
    }

    inline int getcallbackvalue(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TSTRING);

        auto Instance = *(uintptr_t*)lua_touserdata(L, 1);

        int Atom;
        auto Property = lua_tostringatom(L, 2, &Atom);
        std::string property_name(Property);

        const auto Prop = GetInstancePropertyByName2(Instance, property_name);
        if (!Prop)
            luaL_error(L, "this property does not exist");

        const auto callback_instance_v1 = (Instance + *(uintptr_t*)(Prop + 0x78)); // 0x78
        const auto hasCallback = *reinterpret_cast<uintptr_t*>(callback_instance_v1 + 0x38);
        if (!hasCallback) {
            lua_pushnil(L);
            return 1;
        }

        const auto callback_instance_v2 = *reinterpret_cast<uintptr_t*>(callback_instance_v1 + 0x18); // doesnt change 
        const auto callback_instance_v3 = callback_instance_v2 ? *reinterpret_cast<uintptr_t*>(callback_instance_v2 + 0x38) : NULL; // doesnt change 
        const auto callback_instance_v4 = callback_instance_v3 ? *reinterpret_cast<uintptr_t*>(callback_instance_v3 + 0x28) : NULL; // doesnt change 
        const auto callback_id = callback_instance_v4 ? *reinterpret_cast<int*>(callback_instance_v4 + 0x14) : NULL; // doesnt change 
        if (!callback_id) {
            lua_pushnil(L);
            return 1;
        }

        lua_getref(L, callback_id);
        if (lua_isfunction(L, -1))
            return 1;

        lua_pop(L, 1);

        lua_pushnil(L);
        return 1;
    }

    inline int getscriptfromthread(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TTHREAD);

        lua_State* thread = lua_tothread(L, 1);

        if (!thread || !thread->userdata || thread->userdata->Script.expired()) {
            lua_pushnil(L);
            return 1;
        }

        auto script = thread->userdata->Script.lock();
        PushInstanceWeakPtr(L, script);

        return 1;
    }

    __forceinline int getscripts(lua_State* L) {
        struct instancecontext {
            lua_State* L;
            __int64 n;
        } Context = { L, 0 };

        lua_createtable(L, 0, 0);

        const auto ullOldThreshold = L->global->GCthreshold;
        L->global->GCthreshold = SIZE_MAX;

        luaM_visitgco(L, &Context, [](void* ctx, lua_Page* page, GCObject* gco) -> bool {
            auto gCtx = static_cast<instancecontext*>(ctx);
            const auto type = gco->gch.tt;

            if (isdead(gCtx->L->global, gco))
                return false;

            if (type == LUA_TUSERDATA) {

                TValue* top = gCtx->L->top;
                top->value.p = reinterpret_cast<void*>(gco);
                top->tt = type;
                gCtx->L->top++;

                if (!strcmp(luaL_typename(gCtx->L, -1), "Instance")) {
                    lua_getfield(gCtx->L, -1, "ClassName");

                    const char* inst_class = lua_tolstring(gCtx->L, -1, 0);
                    if (!strcmp(inst_class, "LocalScript") || !strcmp(inst_class, "ModuleScript") ||
                        !strcmp(inst_class, "CoreScript") || !strcmp(inst_class, "Script")) {
                        lua_pop(gCtx->L, 1);
                        gCtx->n++;
                        lua_rawseti(gCtx->L, -2, gCtx->n);
                    }
                    else
                        lua_pop(gCtx->L, 2);

                }
                else {
                    lua_pop(gCtx->L, 1);
                }
            }

            return true;
            });

        L->global->GCthreshold = ullOldThreshold;

        return 1;
    }

    inline int getloadedmodules1(lua_State* L)
    {
        lua_newtable(L);

        typedef struct {
            lua_State* pLua;
            int itemsFound;
            std::map< uintptr_t, bool > map;
        } GCOContext;

        auto gcCtx = GCOContext{ L, 0 };

        const auto ullOldThreshold = L->global->GCthreshold;
        L->global->GCthreshold = SIZE_MAX;

        luaM_visitgco(L, &gcCtx, [](void* ctx, lua_Page* pPage, GCObject* pGcObj) -> bool {
            const auto pCtx = static_cast<GCOContext*>(ctx);
            const auto ctxL = pCtx->pLua;

            if (isdead(ctxL->global, pGcObj))
                return false;

            if (const auto gcObjType = pGcObj->gch.tt;
                gcObjType == LUA_TFUNCTION) {
                ctxL->top->value.gc = pGcObj;
                ctxL->top->tt = gcObjType;
                ctxL->top++;

                lua_getfenv(ctxL, -1);

                if (!lua_isnil(ctxL, -1)) {
                    lua_getfield(ctxL, -1, "script");

                    if (!lua_isnil(ctxL, -1)) {
                        uintptr_t script_addr = *(uintptr_t*)lua_touserdata(ctxL, -1);

                        std::string class_name = **(std::string**)(*(uintptr_t*)(script_addr + 0x18) + 0x8);

                        if (pCtx->map.find(script_addr) == pCtx->map.end() && class_name == "ModuleScript") {
                            pCtx->map.insert({ script_addr, true });
                            lua_rawseti(ctxL, -4, ++pCtx->itemsFound);
                        }
                        else {
                            lua_pop(ctxL, 1);
                        }
                    }
                    else {
                        lua_pop(ctxL, 1);
                    }
                }

                lua_pop(ctxL, 2);
            }
            return false;
            });

        L->global->GCthreshold = ullOldThreshold;

        return 1;
    }

    inline int getrunningscriptsv2(lua_State* L)
    {
        lua_newtable(L);
        int tableIndex = lua_gettop(L);
        int i = 0;
        std::unordered_map<std::shared_ptr<std::uintptr_t*>, bool> running_scripts = { };
        lua_rawcheckstack(L, 5);
        luaC_threadbarrier(L);
        lua_pushvalue(L, LUA_REGISTRYINDEX);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_isthread(L, -1)) {
                lua_State* thread = lua_tothread(L, -1);
                if (thread && thread->userdata != nullptr && !thread->userdata->Script.expired()) {
                    if (thread->global->mainthread == L->global->mainthread) {
                        const auto script = *reinterpret_cast<std::shared_ptr<std::uintptr_t*>*>(&thread->userdata->Script);
                        if (!running_scripts.contains(script)) {
                            Roblox::PushInstance2(L, script);
                            lua_getfield(L, -1, "ClassName");
                            const char* className = lua_tostring(L, -1);
                            lua_pop(L, 1);

                            bool isValidScript = false;

                            if (className) {
                                if (strcmp(className, "LocalScript") == 0 || strcmp(className, "ModuleScript") == 0) {
                                    isValidScript = true;
                                }
                                else if (strcmp(className, "Script") == 0) {
                                    lua_getfield(L, -1, "RunContext");
                                    if (lua_type(L, -1) != LUA_TNIL) {
                                        if (lua_isnumber(L, -1)) {
                                            int runContext = lua_tointeger(L, -1);
                                            isValidScript = (runContext == 2);
                                        }
                                        else {
                                            lua_getfield(L, -1, "Value");
                                            if (lua_isnumber(L, -1)) {
                                                int runContext = lua_tointeger(L, -1);
                                                isValidScript = (runContext == 2);
                                            }
                                            lua_pop(L, 1);
                                        }
                                    }
                                    lua_pop(L, 1);
                                }
                            }

                            if (isValidScript) {
                                running_scripts[script] = true;
                                lua_rawseti(L, tableIndex, ++i);
                            }
                            else {
                                lua_pop(L, 1);
                            }
                        }
                    }
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        return 1;
    }

    inline int getscriptclosure(lua_State* L)
    {
        if (!lua_isuserdata(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        std::string ud_t = luaL_typename(L, 1);
        if (ud_t != "Instance") {
            lua_pushnil(L);
            return 1;
        }

        auto script = *reinterpret_cast<std::uintptr_t*>(lua_touserdata(L, 1));
        if (!script) {
            lua_pushnil(L);
            return 1;
        }

        const char* className = *reinterpret_cast<const char**>(
            *reinterpret_cast<uintptr_t*>(script + Offsets::Instance::ClassDescriptor) +
            Offsets::Instance::ClassName
            );

        if (!className) {
            lua_pushnil(L);
            return 1;
        }

        std::uintptr_t protected_string = 0;

        if (strcmp(className, "LocalScript") == 0 || strcmp(className, "Script") == 0) {
            protected_string = *reinterpret_cast<std::uintptr_t*>(script + Offsets::Scripts::LocalScriptByteCode);
        }
        else if (strcmp(className, "ModuleScript") == 0) {
            protected_string = *reinterpret_cast<std::uintptr_t*>(script + Offsets::Scripts::ModuleScriptByteCode);
        }
        else {
            lua_pushnil(L);
            return 1;
        }

        if (!protected_string || IsBadReadPtr(reinterpret_cast<const void*>(protected_string), 0x30)) {
            lua_pushnil(L);
            return 1;
        }

        char* data_ptr = *reinterpret_cast<char**>(protected_string + 0x10);
        size_t data_size = *reinterpret_cast<size_t*>(protected_string + 0x20);

        if (data_size <= 15) {
            data_ptr = reinterpret_cast<char*>(protected_string + 0x10);
        }

        if (!data_ptr || data_size == 0 || data_size > 0x10000000) {
            lua_pushnil(L);
            return 1;
        }

        if (IsBadReadPtr(data_ptr, data_size)) {
            lua_pushnil(L);
            return 1;
        }

        std::string compressedBytecode(data_ptr, data_size);

        if (compressedBytecode.empty() || compressedBytecode.size() < 8) {
            lua_pushnil(L);
            return 1;
        }

        lua_State* scriptThread = lua_newthread(L);
        if (!scriptThread) {
            lua_pushnil(L);
            return 1;
        }

        lua_pop(L, 1);

        luaL_sandboxthread(scriptThread);

        TaskScheduler::SetThreadCapabilities(scriptThread, 8, MaxCapabilities);

        lua_pushvalue(L, 1);
        lua_xmove(L, scriptThread, 1);
        lua_setglobal(scriptThread, "script");

        std::string chunkName = ("=") + std::string(className);
        int loadResult = Roblox::LuaVMLoad(
            scriptThread,
            &compressedBytecode,
            chunkName.c_str(),
            0
        );

        if (loadResult != LUA_OK) {
            lua_pop(scriptThread, lua_gettop(scriptThread));
            lua_pushnil(L);
            return 1;
        }

        if (!lua_isLfunction(scriptThread, -1)) {
            lua_pop(scriptThread, lua_gettop(scriptThread));
            lua_pushnil(L);
            return 1;
        }

        Closure* closure = clvalue(luaA_toobject(scriptThread, -1));
        if (!closure || !closure->isC == 0) {
            lua_pop(scriptThread, lua_gettop(scriptThread));
            lua_pushnil(L);
            return 1;
        }

        Proto* proto = closure->l.p;
        if (!proto) {
            lua_pop(scriptThread, lua_gettop(scriptThread));
            lua_pushnil(L);
            return 1;
        }

        TaskScheduler::SetProtoCapabilities(proto, &MaxCapabilities);

        lua_pop(scriptThread, lua_gettop(scriptThread));

        setclvalue(L, L->top, closure);
        incr_top(L);

        return 1;
    }

    inline int getcallingscript(lua_State* L)
    {
        CallInfo* ci = L->ci;

        while (ci > L->base_ci) {
            if (ci->func && ttisfunction(ci->func)) {
                Closure* cl = clvalue(ci->func);

                bool isExecutor = false;
                for (auto* execCl : Environment::function_array) {
                    if (execCl == cl) {
                        isExecutor = true;
                        break;
                    }
                }

                if (!isExecutor && !cl->isC) {
                    lua_State* thread = L;
                    if (ci->func <= thread->stack) {
                        thread = thread->global->mainthread;
                    }

                    if (thread->userdata && !thread->userdata->Script.expired()) {
                        auto script = thread->userdata->Script.lock();
                        PushInstanceWeakPtr(L, script);
                        return 1;
                    }
                }
            }
            ci--;
        }

        lua_pushnil(L);
        return 1;
    }

    inline bool lua_isscriptable(lua_State* L, uintptr_t property)
    {
        if (!property)
            return false;

        uint64_t bit_flags = *reinterpret_cast<uint64_t*>(property + Offsets::Instance::PropertyDescriptorBitFlags);
        return (bit_flags & Offsets::Instance::ScriptableMask) != 0;
    }

    inline void lua_setscriptable(lua_State* L, uintptr_t property, bool scriptable)
    {
        if (!property)
            return;

        uint64_t* bit_flags_ptr = reinterpret_cast<uint64_t*>(property + Offsets::Instance::PropertyDescriptorBitFlags);
        uint64_t current_flags = *bit_flags_ptr;

        if (scriptable) {
            *bit_flags_ptr = current_flags | Offsets::Instance::ScriptableMask;
        }
        else {
            *bit_flags_ptr = current_flags & ~Offsets::Instance::ScriptableMask;
        }
    }

    inline uintptr_t get_instance_prop_by_name(uintptr_t instance, const std::string& target_property)
    {
        if (!instance) return 0;

        const uintptr_t class_descriptor = *reinterpret_cast<uintptr_t*>(instance + 0x18);
        if (!class_descriptor) return 0;

        const uintptr_t properties_start = *reinterpret_cast<uintptr_t*>(class_descriptor + 0x28);
        if (!properties_start) return 0;

        if (properties_start)
        {
            for (uint32_t i = 0; i < 256; ++i)
            {
                const uintptr_t entry = properties_start + (i * 0x10);
                const uintptr_t prop = *reinterpret_cast<uintptr_t*>(entry);
                if (!prop) continue;

                const uintptr_t name_ptr = *reinterpret_cast<uintptr_t*>(prop + 0x8);
                if (!name_ptr) continue;

                const char* name = reinterpret_cast<const char*>(name_ptr);
                if (name && target_property == name)
                    return prop;
            }
        }

        return 0;
    }

    inline static void check_instance(lua_State* l, int idx) {
        std::string typeoff = luaL_typename(l, idx);
        if (typeoff != "Instance")
            luaL_typeerrorL(l, 1, "Instance");
    }

    inline int gethiddenproperty(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        luaL_checktype(L, 2, LUA_TSTRING);

        const std::string ud_t = luaL_typename(L, 1);
        if (ud_t != "Instance")
            luaL_typeerrorL(L, 1, "Instance");

        const uintptr_t instance = *static_cast<uintptr_t*>(lua_touserdata(L, 1));
        const std::string property_name = lua_tostring(L, 2);

        uintptr_t property = get_instance_prop_by_name(instance, property_name);
        if (!property)
            luaL_argerrorL(L, 2, "property does not exist");

        const uintptr_t get_set_impl = *reinterpret_cast<uintptr_t*>(property + 0x90);
        const uintptr_t get_set_vft = *reinterpret_cast<uintptr_t*>(get_set_impl);
        const uintptr_t getter = *reinterpret_cast<uintptr_t*>(get_set_vft + 0x18);

        const uintptr_t ttype = *reinterpret_cast<uintptr_t*>(property + 0x60);
        const int type_number = *reinterpret_cast<int*>(ttype + 0x30);

        lua_pushcclosure(L, isscriptable, 0, 0);
        lua_pushvalue(L, 1);
        lua_pushstring(L, property_name.c_str());
        lua_call(L, 2, 1);

        const bool is_scriptable = lua_toboolean(L, -1);

        if (type_number == reflection::reflection_type::ReflectionType_Bool) {
            const auto bool_getter = reinterpret_cast<bool(__fastcall*)(uintptr_t, uintptr_t)>(getter);
            const bool result = bool_getter(get_set_impl, instance);

            lua_pushboolean(L, result);
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_Int) {
            const auto int_getter = reinterpret_cast<int(__fastcall*)(uintptr_t, uintptr_t)>(getter);
            const int result = int_getter(get_set_impl, instance);

            lua_pushinteger(L, result);
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_Float) {
            const auto float_getter = reinterpret_cast<float(__fastcall*)(uintptr_t, uintptr_t)>(getter);
            const float result = float_getter(get_set_impl, instance);

            lua_pushnumber(L, result); lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_String) {
            const auto string_getter = reinterpret_cast<void(__fastcall*)(uintptr_t, std::string*, uintptr_t)>(getter);
            std::string result{};
            string_getter(get_set_impl, &result, instance);

            Roblox::Print(1, "getter: %p", (getter - (uintptr_t)GetModuleHandleA(0)));
            lua_pushlstring(L, result.data(), result.size());
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_BinaryString) {
            const auto string_getter = reinterpret_cast<void(__fastcall*)(uintptr_t, uintptr_t*, uintptr_t)>(getter);
            auto* result = new uintptr_t[100];
            string_getter(get_set_impl, result, instance);

            const std::string res = *reinterpret_cast<std::string*>(result);
            Roblox::Print(1, "result: %p", *result);
            lua_pushstring(L, res.c_str());
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_SharedString || type_number == reflection::ReflectionType_DateTime) {
            const auto shared_string_getter = reinterpret_cast<void(__fastcall*)(uintptr_t, reflection::shared_str**, uintptr_t)>(getter);
            reflection::shared_str* result{};
            shared_string_getter(get_set_impl, &result, instance);
            lua_pushlstring(L, result->content.c_str(), result->content.size());
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_SystemAddress) {
            const auto system_address_getter = reinterpret_cast<float(__fastcall*)(uintptr_t, reflection::sys_addr*, uintptr_t)>(getter);
            reflection::sys_addr result{};
            system_address_getter(get_set_impl, &result, instance);

            lua_pushinteger(L, result.remote_id.peer_id);
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else if (type_number == reflection::ReflectionType_Enum) {
            const auto enum_getter = reinterpret_cast<int(__fastcall*)(uintptr_t, uintptr_t)>(getter);
            const int result = enum_getter(get_set_impl, instance);
            lua_pushinteger(L, result);
            lua_pushboolean(L, !is_scriptable);
            return 2;
        }
        else {
            lua_pushstring(L, property_name.c_str());
        }
        lua_pushboolean(L, !is_scriptable);
        return 2;
    }

    inline int sethiddenproperty(lua_State* LS) {
        luaL_checktype(LS, 1, LUA_TUSERDATA);
        luaL_checktype(LS, 2, LUA_TSTRING);
        luaL_checkany(LS, 3);

        check_instance(LS, 1);

        std::string PropName = lua_tostring(LS, 2);

        lua_pushcclosure(LS, isscriptable, 0, 0);
        lua_pushvalue(LS, 1);
        lua_pushstring(LS, PropName.c_str());
        lua_call(LS, 2, 1);

        const bool was = lua_toboolean(LS, -1);
        lua_pop(LS, 1);

        lua_pushcclosure(LS, setscriptable, 0, 0);
        lua_pushvalue(LS, 1);
        lua_pushstring(LS, PropName.c_str());
        lua_pushboolean(LS, true);
        lua_pcall(LS, 3, 1, 0);
        lua_pop(LS, 1);

        lua_pushvalue(LS, 3);
        lua_setfield(LS, 1, PropName.c_str());
        lua_pushboolean(LS, !was);

        lua_pushcclosure(LS, setscriptable, 0, 0);
        lua_pushvalue(LS, 1);
        lua_pushstring(LS, PropName.c_str());
        lua_pushboolean(LS, was);
        lua_pcall(LS, 3, 1, 0);
        lua_pop(LS, 1);

        return 1;
    };

    inline void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        Utils::AddFunction(L, "isscriptable", isscriptable);
        Utils::AddFunction(L, "setscriptable", setscriptable);

        Utils::AddFunction(L, "gethiddenproperty", gethiddenproperty);
        Utils::AddFunction(L, "sethiddenproperty", sethiddenproperty);

        Utils::AddFunction(L, "gethiddenprop", gethiddenproperty);
        Utils::AddFunction(L, "sethiddenprop", sethiddenproperty);

        Utils::AddFunction(L, "getscripthash", getscripthash);

        Utils::AddFunction(L, "getscriptbytecode", getscriptbytecode);
        Utils::AddFunction(L, "dumpstring", getscriptbytecode);

        Utils::AddFunction(L, "decompile", decompile);

        Utils::AddFunction(L, "getscriptfromthread", getscriptfromthread);
        Utils::AddFunction(L, "getscripts", getscripts);
        Utils::AddFunction(L, "getloadedmodules", getloadedmodules1);
        Utils::AddFunction(L, "getrunningscripts", getrunningscriptsv2);

        //Utils::AddFunction(L, "getcallbackvalue", getcallbackvalue);

        Utils::RegisterAliases(L, getcallbackvalue, { "getcallbackvalue", "getcallbackmember" });

        Utils::AddFunction(L, "getscriptclosure", getscriptclosure);
        Utils::AddFunction(L, "getscriptfunction", getscriptclosure);

        Utils::AddFunction(L, "getcallingscript", getcallingscript);

    }
}