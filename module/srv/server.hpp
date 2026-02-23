#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>

namespace Communication
{
	void Initialize();
}