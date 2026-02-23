#pragma once

#include <windows.h>
#include <lstate.h>
#include <lualib.h>

namespace TaskScheduler
{
	bool SetupExploit();
	void RequestExecution(std::string Script);
	void SetProtoCapabilities(Proto* Proto, uintptr_t* Capabilities);
	void SetThreadCapabilities(lua_State* L, int Level, uintptr_t Capabilities);

	uintptr_t GetDataModel();
	uintptr_t GetScriptContext(uintptr_t DataModel);
	lua_State* GetLuaStateForInstance(uintptr_t ScriptContext);
}