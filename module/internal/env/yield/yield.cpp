#include <internal/globals.hpp>
#include <internal/env/yield/yield.hpp>

void Yielding::RunYield()
{
	if (!SharedVariables::YieldQueue.empty())
	{
		std::function<void()> YieldingRequest = SharedVariables::YieldQueue.front();
		SharedVariables::YieldQueue.pop();
		YieldingRequest();
	}
}

int Yielding::YieldExecution(lua_State* L, const std::function<Yielded()>& YieldingClosure)
{
	lua_pushthread(L);
	int YieldedThreadRef = lua_ref(L, -1);
	lua_pop(L, 1);

	std::thread([=]
	{
		Yielded ResumeFunction = YieldingClosure();

		SharedVariables::YieldQueue.emplace([=]() -> void
		{
			WeakThreadRef WeakThreadRefObject = { L };
			lua_pushthread(L);
			WeakThreadRefObject.ThreadRef = YieldedThreadRef;
			lua_pop(L, 1);
			WeakThreadRef* WeakThreadRef = &WeakThreadRefObject;

			DebuggerResult DebuggerResult = { 0 };
			auto ScriptContext = reinterpret_cast<uintptr_t>(L->userdata->SharedExtraSpace->ScriptContext);
			Roblox::ScriptContextResume(ScriptContext + Offsets::ExtraSpace::ScriptContextToResume, &DebuggerResult, &WeakThreadRef, ResumeFunction(L), NULL, NULL);

			lua_unref(L, YieldedThreadRef);
		});
	}).detach();

	return lua_yield(L, 0);
}