#include <internal/env/env.hpp>
#include <internal/env/yield/yield.hpp>
#include <internal/execution/execution.hpp>
#include <internal/globals.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>


static void crash_log_s(const char *msg) {
  FILE *f = nullptr;
  fopen_s(&f, "C:\\Users\\Admin\\Desktop\\vicna_crash.log", "a");
  if (f) {
    fprintf(f, "%s\n", msg);
    fflush(f);
    fclose(f);
  }
}

void TaskScheduler::SetProtoCapabilities(Proto *Proto,
                                         uintptr_t *Capabilities) {
  Proto->userdata = Capabilities;
  for (int i = 0; i < Proto->sizep; ++i)
    SetProtoCapabilities(Proto->p[i], Capabilities);
}

void TaskScheduler::SetThreadCapabilities(lua_State *L, int Level,
                                          uintptr_t Capabilities) {
  L->userdata->Identity = Level;
  L->userdata->Capabilities = Capabilities;
}

uintptr_t TaskScheduler::GetDataModel() {
  uintptr_t FakeDataModel =
      *reinterpret_cast<uintptr_t *>(Offsets::DataModel::FakeDataModelPointer);
  uintptr_t DataModel = *reinterpret_cast<uintptr_t *>(
      FakeDataModel + Offsets::DataModel::FakeDataModelToDataModel);

  return DataModel;
}

uintptr_t TaskScheduler::GetScriptContext(uintptr_t DataModel) {
  uintptr_t Children =
      *reinterpret_cast<uintptr_t *>(DataModel + Offsets::DataModel::Children);
  uintptr_t ScriptContext =
      *reinterpret_cast<uintptr_t *>(*reinterpret_cast<uintptr_t *>(Children) +
                                     Offsets::DataModel::ScriptContext);

  return ScriptContext;
}

lua_State *TaskScheduler::GetLuaStateForInstance(uintptr_t Instance) {
  *reinterpret_cast<BOOLEAN *>(Instance + Offsets::ExtraSpace::RequireBypass) =
      TRUE;

  uint64_t Null = 0;
  return Roblox::GetLuaStateForInstance(Instance, &Null, &Null);
}

int ScriptsHandler(lua_State *L) {
  if (!SharedVariables::ExecutionRequests.empty()) {
    Execution::ExecuteScript(SharedVariables::ExploitThread,
                             SharedVariables::ExecutionRequests.front());
    SharedVariables::ExecutionRequests.erase(
        SharedVariables::ExecutionRequests.begin());
  }
  Yielding::RunYield();

  return 0;
}

void SetupExecution(lua_State *L) {
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

bool TaskScheduler::SetupExploit() {
  crash_log_s("[5a] GetScriptContext");
  uintptr_t ScriptContext =
      TaskScheduler::GetScriptContext(SharedVariables::LastDataModel);
  crash_log_s("[5b] GetLuaStateForInstance");
  lua_State *RobloxState = TaskScheduler::GetLuaStateForInstance(ScriptContext);

  crash_log_s("[5c] lua_newthread");
  SharedVariables::ExploitThread = lua_newthread(RobloxState);
  crash_log_s("[5d] SetThreadCapabilities");
  TaskScheduler::SetThreadCapabilities(SharedVariables::ExploitThread, 8,
                                       MaxCapabilities);
  crash_log_s("[5e] SetupEnvironment");
  Environment::SetupEnvironment(SharedVariables::ExploitThread);
  crash_log_s("[5f] SetupExecution");
  SetupExecution(SharedVariables::ExploitThread);
  crash_log_s("[5g] SetupExploit done");

  return true;
}

void TaskScheduler::RequestExecution(std::string Script) {
  SharedVariables::ExecutionRequests.push_back(Script);
}