#pragma once

#include <Windows.h>
#include <lstate.h>
#include <lualib.h>
#include <vector>

namespace Environment
{
    extern std::vector<Closure*> function_array;

    void SetupEnvironment(lua_State* L);
}