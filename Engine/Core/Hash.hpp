#pragma once

#include "Macros.hpp"
#include <stdint.h>
#include <vector>

namespace Engine
{
	class Hash
	{
	public:
		EXPORT static uint64_t CalculateHash(const void* data, uint64_t length);
		EXPORT static uint64_t CalculateHash(const std::vector<uint8_t>& vector);
	};
}