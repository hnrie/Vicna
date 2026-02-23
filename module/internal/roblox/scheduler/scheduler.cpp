#include <internal/globals.hpp>
#include <internal/execution/execution.hpp>
#include <internal/env/env.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>
#include <internal/env/yield/yield.hpp>

void TaskScheduler::SetProtoCapabilities(Proto* Proto, uintptr_t* Capabilities)
{
    Proto->userdata = Capabilities;
    for (int i = 0; i < Proto->sizep; ++i)
        SetProtoCapabilities(Proto->p[i], Capabilities);
}

void TaskScheduler::SetThreadCapabilities(lua_State* L, int Level, uintptr_t Capabilities)
{
    L->userdata->Identity = Level;
    L->userdata->Capabilities = Capabilities;
}

uintptr_t TaskScheduler::GetDataModel()
{
	uintptr_t FakeDataModel = *reinterpret_cast<uintptr_t*>(Offsets::DataModel::FakeDataModelPointer);
    uintptr_t DataModel = *reinterpret_cast<uintptr_t*>(FakeDataModel + Offsets::DataModel::FakeDataModelToDataModel);

	return DataModel;
}

uintptr_t TaskScheduler::GetScriptContext(uintptr_t DataModel)
{
    uintptr_t Children = *reinterpret_cast<uintptr_t*>(DataModel + Offsets::DataModel::Children);
	uintptr_t ScriptContext = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(Children) + Offsets::DataModel::ScriptContext);

    return ScriptContext;
}

lua_State* TaskScheduler::GetLuaStateForInstance(uintptr_t Instance)
{
    *reinterpret_cast<BOOLEAN*>(Instance + Offsets::ExtraSpace::RequireBypass) = TRUE;

    uint64_t Null = 0;
    return Roblox::GetLuaStateForInstance(Instance, &Null, &Null);
}

int ScriptsHandler(lua_State* L)
{
    if (!SharedVariables::ExecutionRequests.empty())
    {
        Execution::ExecuteScript(SharedVariables::ExploitThread, SharedVariables::ExecutionRequests.front());
        SharedVariables::ExecutionRequests.erase(SharedVariables::ExecutionRequests.begin());
    }
    Yielding::RunYield();

    return 0;
}

void SetupExecution(lua_State* L)
{
    lua_getglobal(L, "game");
    lua_getfield(L, -1, "GetService");
    lua_pushvalue(L, -2);

    lua_pushstring(L, "RunService");
    lua_pcall(L, 2, 1, 0);

    lua_getfield(L, -1, "RenderStepped");
    lua_getfield(L, -1, "Connect");
    lua_pushvalue(L, -2);

    lua_pushcclosure(L, ScriptsHandler, nullptr, 0);
    lua_pcall(L, 2, 0, 0);
    lua_pop(L, 2);
}

bool TaskScheduler::SetupExploit()
{
	uintptr_t ScriptContext = TaskScheduler::GetScriptContext(SharedVariables::LastDataModel);
    lua_State* RobloxState = TaskScheduler::GetLuaStateForInstance(ScriptContext);

    SharedVariables::ExploitThread = lua_newthread(RobloxState);
	TaskScheduler::SetThreadCapabilities(SharedVariables::ExploitThread, 8, MaxCapabilities);
	Environment::SetupEnvironment(SharedVariables::ExploitThread);
    SetupExecution(SharedVariables::ExploitThread);

    return true;
}

void TaskScheduler::RequestExecution(std::string Script)
{
    SharedVariables::ExecutionRequests.push_back(Script);
}