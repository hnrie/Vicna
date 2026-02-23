#include "bytecode.hpp"

#include <zstd/zstd.h>
#include <zstd/xxhash.h>

#include <vector>
#include <internal/roblox/offsets/offsets.hpp>

std::string Bytecode::RequestBytecode(std::uintptr_t scriptPtr)
{
	if (!scriptPtr) return "";

	const char* ClassName = *reinterpret_cast<const char**>(
		*reinterpret_cast<uintptr_t*>(scriptPtr + Offsets::Instance::ClassDescriptor) +
		Offsets::Instance::ClassName
		);

	if (!ClassName) return "";

	std::uintptr_t bytecodePtr = 0;

	if (strcmp(ClassName, "LocalScript") == 0) {
		bytecodePtr = *reinterpret_cast<std::uintptr_t*>(scriptPtr + Offsets::Scripts::LocalScriptByteCode);
	}
	else if (strcmp(ClassName, "ModuleScript") == 0) {
		bytecodePtr = *reinterpret_cast<std::uintptr_t*>(scriptPtr + Offsets::Scripts::ModuleScriptByteCode);
	}
	else {
		return ""; // script or other unsupported type
	}

	if (!bytecodePtr || IsBadReadPtr(reinterpret_cast<const void*>(bytecodePtr), 0x30)) {
		return "";
	}

	const char* data = *reinterpret_cast<const char**>(bytecodePtr + 0x10);
	size_t size = *reinterpret_cast<size_t*>(bytecodePtr + 0x20);

	if (!data || size == 0 || size > 0x10000000) {
		return "";
	}

	if (IsBadReadPtr(data, size)) {
		return "";
	}

	std::string compressed(data, size);
	return decompress_bytecode(compressed);
}

std::string Bytecode::ReadByteCode(std::uint64_t Address) {
	std::uintptr_t str = Address + 0x10;
	std::uintptr_t data;

	if (*reinterpret_cast<std::size_t*>(str + 0x18) > 0xf) {
		data = *reinterpret_cast<std::uintptr_t*>(str);
	}
	else {
		data = str;
	}

	std::string BOOOHOOOOOOOO;
	std::size_t len = *reinterpret_cast<std::size_t*>(str + 0x10);
	BOOOHOOOOOOOO.reserve(len);

	for (unsigned i = 0; i < len; i++) {
		BOOOHOOOOOOOO += *reinterpret_cast<char*>(data + i);
	}

	return BOOOHOOOOOOOO;
}

std::string Bytecode::RequestCompressedBytecode(std::uintptr_t script)
{
	//uintptr_t code[0x4];
	//std::memset(code, 0, sizeof(code));

	//roblox::rbx_request_code((std::uintptr_t)code, script);

	//std::uintptr_t bytecodePtr = code[1];

	//if (!bytecodePtr) { return (("Failed to get bytecode: invalid bytecode ptr")); }

	//std::string Bytecode = ReadByteCode(bytecodePtr);
	//if (Bytecode.size() <= 8) { return (("Failed to get bytecode: invalid bytecode size")); }

	//return Bytecode;

	const char* ClassName = *reinterpret_cast<const char**>(*reinterpret_cast<uintptr_t*>(script +
		Offsets::Instance::ClassDescriptor) +
		Offsets::Instance::ClassName);

	std::string Bytecode = "";

	if (ClassName && strcmp(ClassName, "LocalScript") == 0)
	{
		const auto BytecodeStrPtr = *(uintptr_t*)(script + Offsets::Scripts::LocalScriptByteCode);
		Bytecode = *(std::string*)(BytecodeStrPtr + Offsets::Scripts::Bytecode);

		if (Bytecode.size() <= 8)
		{
			Bytecode = "";
		}
	}
	else if (ClassName && strcmp(ClassName, "ModuleScript") == 0)
	{
		const auto BytecodeStrPtr = *(uintptr_t*)(script + Offsets::Scripts::ModuleScriptByteCode);
		Bytecode = *(std::string*)(BytecodeStrPtr + 0x10);

		if (Bytecode.size() <= 8)
		{
			Bytecode = "";
		}
	}
	else if (ClassName && strcmp(ClassName, "Script") == 0)
	{
		Bytecode = "";
	}
	else
	{
		Bytecode = "";
		// idk
	}

	return Bytecode;
}


std::string Bytecode::decompress_bytecode(const std::string& source)
{
	constexpr const char bytecode_magic[] = "RSB1";

	std::string input = source;

	std::uint8_t hash_bytes[4];
	memcpy(hash_bytes, &input[0], 4);

	for (auto i = 0u; i < 4; ++i) {
		hash_bytes[i] ^= bytecode_magic[i];
		hash_bytes[i] -= i * 41;
	}

	for (size_t i = 0; i < input.size(); ++i)
		input[i] ^= hash_bytes[i % 4] + i * 41;

	XXH32(&input[0], input.size(), 42);

	std::uint32_t data_size;
	memcpy(&data_size, &input[4], 4);
	std::vector<std::uint8_t> data(data_size);
	ZSTD_decompress(&data[0], data_size, &input[8], input.size() - 8);

	return std::string(reinterpret_cast<char*>(&data[0]), data_size);
}