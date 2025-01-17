#pragma once

#include "Core/Macros.hpp"
#include <vector>
#include <string>

namespace Engine::UI
{
	struct ScrollingGraphBuffer
	{
		EXPORT ScrollingGraphBuffer(const std::string& label, uint32_t m_maxSize);
		EXPORT void SetValue(uint32_t index, float value);
		EXPORT void Clear();

		std::string Label;
		std::vector<float> Values;
		const uint32_t Capacity;
	};

}