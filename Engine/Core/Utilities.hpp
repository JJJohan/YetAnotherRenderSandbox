#pragma once

#include <algorithm>
#include <ranges>
#include <string>

namespace Engine
{
	class Utilities
	{
	public:
		inline static bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
		{
			auto to_lower{ std::ranges::views::transform(static_cast<int(*)(int)>(std::tolower)) };
			return std::ranges::equal(lhs | to_lower, rhs | to_lower);
		}

		inline static void StringToLower(std::string& data)
		{
			std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) { return std::tolower(c); });
		}
	};
}