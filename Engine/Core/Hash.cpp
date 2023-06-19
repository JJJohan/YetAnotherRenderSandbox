#include "Hash.hpp"
#include <string>

namespace Engine
{
	uint64_t Hash::CalculateHash(const void* data, uint64_t length)
	{
		return std::hash<std::string_view>{}({ reinterpret_cast<const char*>(data), length });
	}

	uint64_t Hash::CalculateHash(const std::vector<uint8_t>& vector)
	{
		return std::hash<std::string_view>{}({ reinterpret_cast<const char*>(vector.data()), vector.size()});
	}
}