#pragma once

#include <Windows.h>
#include <lstate.h>
#include <string>
#include <vector>
#include <lualib.h>
#include <cstdint>
#include "globals.hpp"
#include <logging/pipe.hxx>

namespace Environment {
	extern std::vector<Closure*> function_array;
}

namespace Utils
{
	inline void EnsureLogReady()
	{
		static bool isReady = false;
		if (!isReady)
		{
			debug_logger::set_debug_mode(true);
			debug_logger::initialize_pipe();
			isReady = true;
		}
	}

	inline int LoggedFunctionDispatch(lua_State* L)
	{
		EnsureLogReady();

		const char* funcName = lua_tostring(L, lua_upvalueindex(2));
		if (funcName)
			debug_logger::printf("env_function_called: %s", funcName);
		else
			debug_logger::printf("env_function_called: <unknown>");

		void* rawPtr = lua_touserdata(L, lua_upvalueindex(1));
		lua_CFunction func = reinterpret_cast<lua_CFunction>(reinterpret_cast<uintptr_t>(rawPtr));
		if (!func)
			return 0;

		return func(L);
	}

	inline void PushLoggedFunction(lua_State* L, const char* Name, lua_CFunction Function)
	{
		EnsureLogReady();

		lua_pushlightuserdata(L, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(Function)));
		lua_pushstring(L, Name ? Name : "<unnamed>");
		lua_pushcclosure(L, LoggedFunctionDispatch, nullptr, 2);
	}

	inline void AddFunction(lua_State* L, const char* Name, lua_CFunction Function)
	{
		PushLoggedFunction(L, Name, Function);
		Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
		Environment::function_array.push_back(closure);
		lua_setglobal(L, Name);
	}

	inline void AddTableFunction(lua_State* L, const char* Name, lua_CFunction Function)
	{
		PushLoggedFunction(L, Name, Function);
		Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
		Environment::function_array.push_back(closure);
		lua_setfield(L, -2, Name);
	}

	inline void RegisterAliases(lua_State* L, lua_CFunction Function, const std::vector<const char*>& Aliases)
	{
		for (const char* alias : Aliases)
		{
			PushLoggedFunction(L, alias, Function);
			Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
			Environment::function_array.push_back(closure);
			lua_setglobal(L, alias);
		}
	}

	inline void RegisterTableAliases(lua_State* L, lua_CFunction Function, const std::vector<const char*>& Aliases)
	{
		for (const char* alias : Aliases)
		{
			PushLoggedFunction(L, alias, Function);
			Closure* closure = *reinterpret_cast<Closure**>(index2addr(L, -1));
			Environment::function_array.push_back(closure);
			lua_setfield(L, -2, alias);
		}
	}

	inline bool IsInGame(uintptr_t DataModel)
	{
		uintptr_t GameLoaded = *reinterpret_cast<uintptr_t*>(DataModel + Offsets::DataModel::GameLoaded);
		if (GameLoaded != 31)
			return false;

		return true;
	}
}
