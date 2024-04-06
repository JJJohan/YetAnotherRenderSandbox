#pragma once

#include "Macros.hpp"
#include <stdint.h>
#include <vector>
#include <string>

namespace Engine
{
	class Base64
	{
	public:
		EXPORT static constexpr std::string Encode(const uint8_t* data, size_t length);
		EXPORT static constexpr std::vector<uint8_t> Decode(const char* data, size_t length = 0);
	};
}