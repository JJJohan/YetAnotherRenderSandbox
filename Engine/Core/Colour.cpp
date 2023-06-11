#pragma once

#include "Colour.hpp"
#include <algorithm>

namespace Engine
{
	Colour::Colour(float r, float g, float b, float a)
	{
		R = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
		G = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
		B = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
		A = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
	}

	Colour::Colour(const glm::vec4& colour)
	{
		R = static_cast<uint8_t>(std::clamp(colour.r, 0.0f, 1.0f) * 255.0f);
		G = static_cast<uint8_t>(std::clamp(colour.g, 0.0f, 1.0f) * 255.0f);
		B = static_cast<uint8_t>(std::clamp(colour.b, 0.0f, 1.0f) * 255.0f);
		A = static_cast<uint8_t>(std::clamp(colour.a, 0.0f, 1.0f) * 255.0f);
	}
}