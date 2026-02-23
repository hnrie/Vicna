#include <internal/globals.hpp>
#include <internal/execution/execution.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>

extern "C" {
#include "blake3/blake3.h"
#include <random>
}

class BytecodeEncoder : public Luau::BytecodeEncoder
{
	inline void encode(uint32_t* data, size_t count) override
	{
		for (auto i = 0; i < count;)
		{
			uint8_t Opcode = LUAU_INSN_OP(data[i]);
			const auto LookupTable = reinterpret_cast<BYTE*>(Offsets::OpcodeLookupTable);
			uint8_t FinalOpcode = Opcode * 227;
			FinalOpcode = LookupTable[FinalOpcode];

			data[i] = (FinalOpcode) | (data[i] & ~0xFF);
			i += Luau::getOpLength(static_cast<LuauOpcode>(Opcode));
		}
	}
};

std::string SignBytecode(const std::string& bytecode)
{
	std::string signedBytecode = bytecode;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

	uint32_t part1 = dist(gen);
	uint32_t part2 = part1 ^ 0x946AC432;

	signedBytecode.append(reinterpret_cast<const char*>(&part1), 4);
	signedBytecode.append(reinterpret_cast<const char*>(&part2), 4);

	uint8_t hash[BLAKE3_OUT_LEN];
	blake3_hasher hasher;
	blake3_hasher_init(&hasher);
	blake3_hasher_update(&hasher, bytecode.data(), bytecode.size());
	blake3_hasher_finalize(&hasher, hash, BLAKE3_OUT_LEN);

	signedBytecode.append(reinterpret_cast<const char*>(hash), BLAKE3_OUT_LEN);

	return signedBytecode;
}

std::string Execution::CompileScript(std::string Source)
{
	auto BytecodeEncoding = BytecodeEncoder();
	static const char* CommonGlobals[] = { "Game", "Workspace", "game", "plugin", "script", "shared", "workspace", "_G", "_ENV", nullptr };

	Luau::CompileOptions Options;
	Options.debugLevel = 1;
	Options.optimizationLevel = 1;
	Options.mutableGlobals = CommonGlobals;
	Options.vectorLib = "Vector3";
	Options.vectorCtor = "new";
	Options.vectorType = "Vector3";

	std::string bytecode = Luau::compile(Source, Options, {}, &BytecodeEncoding);

	return SignBytecode(bytecode);
}

void Execution::ExecuteScript(lua_State* L, std::string Script)
{
	if (Script.empty())
		return;

	int OriginalTop = lua_gettop(L);
	lua_State* ExecutionThread = lua_newthread(L);
	lua_pop(L, 1);

	luaL_sandboxthread(ExecutionThread);
	TaskScheduler::SetThreadCapabilities(ExecutionThread, 8, MaxCapabilities);

	std::string Bytecode = Execution::CompileScript(Script);
	lua_pushcclosure(ExecutionThread, reinterpret_cast<lua_CFunction>(Roblox::TaskDefer), 0, 0);
	if (luau_load(ExecutionThread, "", Bytecode.c_str(), Bytecode.length(), 0) != LUA_OK)
	{
		std::string Error = lua_tostring(ExecutionThread, -1);
		Roblox::Print(3, "%s", Error.c_str());
		lua_pop(ExecutionThread, 1);
		return;
	}

	Closure* Closure = clvalue(luaA_toobject(ExecutionThread, -1));
	TaskScheduler::SetProtoCapabilities(Closure->l.p, &MaxCapabilities);

	if (lua_pcall(ExecutionThread, 1, NULL, NULL) != LUA_OK)
	{
		std::string Error = lua_tostring(ExecutionThread, -1);
		Roblox::Print(3, "%s", Error.c_str());
		lua_pop(ExecutionThread, 1);
		return;
	}

	lua_settop(ExecutionThread, 0);
	lua_settop(L, OriginalTop);
}