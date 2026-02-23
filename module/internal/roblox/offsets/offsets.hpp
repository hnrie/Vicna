#pragma once

#include <Windows.h>
#include <atomic>
#include <string>
#include <memory>
#include <internal/roblox/offsets/helpers/encshelper.hpp>

struct lua_State;
struct WeakThreadRef;
struct DebuggerResult;

#define REBASE(Address) (Address + reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)))

namespace Offsets
{
    const uintptr_t Print = REBASE(0x1B09F20);
    const uintptr_t TaskDefer = REBASE(0x1AF0340);
    const uintptr_t OpcodeLookupTable = REBASE(0x5e36590);
    const uintptr_t ScriptContextResume = REBASE(0x1A3E620);

    const uintptr_t LuaVMLoad = REBASE(0x19840A0);

    const uintptr_t get_capabilites = REBASE(0x3D6F2A0);

    const uintptr_t PushInstance = REBASE(0x1A4B160);
    const uintptr_t PushInstance2 = REBASE(0x1A4B1B0);

    const uintptr_t GetLuaStateForInstance = REBASE(0x1955D60);

    namespace Signals
    {
        const uintptr_t FireProximityPrompt = REBASE(0x224BF00);
        const uintptr_t FireMouseClick = REBASE(0x21DF6E0);
        const uintptr_t FireRightMouseClick = REBASE(0x21DF880);
        const uintptr_t FireMouseHoverEnter = REBASE(0x21E0C80);
        const uintptr_t FireMouseHoverLeave = REBASE(0x21E0E20);
        const uintptr_t FireTouchInterest = REBASE(0x174B154);
    }

    namespace Luau
    {
        const uintptr_t Luau_Execute = REBASE(0x3A40720);
        const uintptr_t LuaO_NilObject = REBASE(0x594BCE8);
        const uintptr_t LuaH_DummyNode = REBASE(0x594B3B8);
    }

    namespace DataModel
    {
        const uintptr_t Children = 0x70;
        const uintptr_t GameLoaded = 0x5F8;
        const uintptr_t ScriptContext = 0x3F0;
        const uintptr_t FakeDataModelToDataModel = 0x1C0;

        const uintptr_t FakeDataModelPointer = REBASE(0x7E35858);
    }

    namespace ExtraSpace
    {
        const uintptr_t RequireBypass = 0x948;
        const uintptr_t ScriptContextToResume = 0x850;
    }

    namespace Instance
    {
        const uintptr_t Primitive = 0x148;
        const uintptr_t Overlap = 0x200;

        const uintptr_t PropertyDescriptorBitFlags = 0x88;
        const uintptr_t ScriptableMask = 0x10;

        const uintptr_t ClassDescriptor = 0x18;
        const uintptr_t ClassName = 0x8;

        const uintptr_t PropDescriptor = 0x2c0;
    }

    namespace Scripts {
        const uintptr_t LocalScriptByteCode = 0x1A8;
        const uintptr_t ModuleScriptByteCode = 0x150;

        const uintptr_t Bytecode = 0x10;
        const uintptr_t BytecodeSize = 0x20;

        const uintptr_t weak_thread_node = 0x180;
        const uintptr_t weak_thread_ref = 0x8;
        const uintptr_t weak_thread_ref_live = 0x20;
        const uintptr_t weak_thread_ref_live_thread = 0x8;
    }

    namespace Identity {
        const uintptr_t GetIdentityStruct = REBASE(0x92F0);
        const uintptr_t IdentityPtr = REBASE(0x78743E8);
    }
}

namespace Roblox
{
    using TGetIdentityStruct = uintptr_t(__fastcall*)(uintptr_t);
    inline auto GetCapabilities = (__int64(__fastcall*)(int*))Offsets::get_capabilites;
    inline auto GetIdentityStruct = reinterpret_cast<TGetIdentityStruct>(Offsets::Identity::GetIdentityStruct);

    inline auto Print = (uintptr_t(*)(int, const char*, ...))Offsets::Print;
    inline auto TaskDefer = (uint64_t(__fastcall*)(lua_State*))Offsets::TaskDefer;
    inline auto Luau_Execute = (void(__fastcall*)(lua_State*))Offsets::Luau::Luau_Execute;
    inline auto GetLuaStateForInstance = (lua_State * (__fastcall*)(uint64_t, uint64_t*, uint64_t*))Offsets::GetLuaStateForInstance;
    inline auto ScriptContextResume = (uint64_t(__fastcall*)(uint64_t, DebuggerResult*, WeakThreadRef**, uint32_t, uint8_t, uint64_t))Offsets::ScriptContextResume;

    inline auto PushInstance = (uintptr_t * (__fastcall*)(lua_State*, uintptr_t))Offsets::PushInstance;
    inline auto PushInstance2 = (uintptr_t * (__fastcall*)(lua_State*, std::shared_ptr<uintptr_t*>))Offsets::PushInstance2;

    inline auto Uintptr_TPushInstance = (uintptr_t * (__fastcall*)(lua_State*, uintptr_t))Offsets::PushInstance;

    typedef int(__fastcall* LuaVMLoad_t)(lua_State* L, std::string* bytecode, const char* chunkname, int env);
    inline LuaVMLoad_t LuaVMLoad = reinterpret_cast<LuaVMLoad_t>(Offsets::LuaVMLoad);

    using fireclick_t = void(__fastcall*)(void* clickDetector, float distance, void* player);
    inline auto FireMouseClick = reinterpret_cast<fireclick_t>(Offsets::Signals::FireMouseClick);
    inline auto FireRightMouseClick = reinterpret_cast<fireclick_t>(Offsets::Signals::FireRightMouseClick);

    using firehover_t = void(__fastcall*)(void* clickDetector, void* player);
    inline auto FireMouseHover = reinterpret_cast<firehover_t>(Offsets::Signals::FireMouseHoverEnter);
    inline auto FireMouseHoverLeave = reinterpret_cast<firehover_t>(Offsets::Signals::FireMouseHoverLeave);

    inline auto FireProximityPrompt = (void(__fastcall*)(uintptr_t))Offsets::Signals::FireProximityPrompt;

    inline auto FireTouchInterest = (void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, bool, bool))Offsets::Signals::FireTouchInterest;
}