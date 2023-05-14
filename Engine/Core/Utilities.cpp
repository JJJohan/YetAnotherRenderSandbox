#pragma once

#include "Utilities.hpp"
#include <algorithm>
#include <ranges>
#include <string>

namespace Engine
{
	bool Utilities::EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
	{
		auto to_lower{ std::ranges::views::transform(static_cast<int(*)(int)>(std::tolower)) };
		return std::ranges::equal(lhs | to_lower, rhs | to_lower);
	}
}