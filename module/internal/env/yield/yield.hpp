#pragma once

#include <functional>
#include <memory>
#include <lua.h>
#include <thread>

struct DebuggerResult
{
    uint32_t Result;
    uint32_t Unk[4];
};

struct WeakThreadRef
{
    std::atomic<uint32_t> Refs;
    lua_State* L;
    uint32_t ThreadRef;
    uint32_t ObjectId;
    uint32_t Unk1;
    uint32_t Unk2;

    WeakThreadRef(lua_State* L) : Refs(0), L(L), ThreadRef(0), ObjectId(0), Unk1(0), Unk2(0) {}
};

using Yielded = std::function<int(lua_State*)>;
namespace Yielding
{
	int YieldExecution(lua_State* L, const std::function<Yielded()>& Function);
	void RunYield();
}