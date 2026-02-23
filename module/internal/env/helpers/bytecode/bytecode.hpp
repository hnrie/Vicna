#pragma once

#include <windows.h>
#include <string>

class Bytecode
{
public:
	static std::string RequestBytecode(std::uintptr_t scriptPtr);

	static std::string ReadByteCode(std::uint64_t Address);

	static std::string RequestCompressedBytecode(std::uintptr_t script);

	static std::string decompress_bytecode(const std::string& source);
};