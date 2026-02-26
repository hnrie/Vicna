#include <internal/globals.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>
#include <internal/utils.hpp>
#include <srv/server.hpp>


static void crash_log(const char *msg) {
  FILE *f = nullptr;
  fopen_s(&f, "C:\\Users\\Admin\\Desktop\\vicna_crash.log", "a");
  if (f) {
    fprintf(f, "%s\n", msg);
    fflush(f);
    fclose(f);
  }
}

void MainThread() {
  crash_log("[1] MainThread started");
  Communication::Initialize();
  crash_log("[2] Communication initialized");

  while (true) {
    uintptr_t DataModel = TaskScheduler::GetDataModel();
    if (!DataModel) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      continue;
    }

    if (SharedVariables::LastDataModel != DataModel) {
      crash_log("[3] New DataModel detected");
      if (!Utils::IsInGame(DataModel)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        continue;
      }
      crash_log("[4] IsInGame=true");
      SharedVariables::LastDataModel = DataModel;
      SharedVariables::ExecutionRequests.clear();

      crash_log("[5] Calling SetupExploit");
      TaskScheduler::SetupExploit();
      crash_log("[6] SetupExploit done");
      TaskScheduler::RequestExecution("warn(\"are we ud?\")");
      crash_log("[7] First script requested");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    std::thread(MainThread).detach();
  }

  return TRUE;
}