#pragma once

#include <stdint.h>
#include "Macros.hpp"
#include <glm/glm.hpp>

namespace Engine
{
	struct EXPORT Colour
	{
		Colour(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
		Colour(const glm::vec4& colour);

		bool operator==(const Colour& other) const
		{
			return R == other.R && G == other.G && B == other.B && A == other.A;
		}

		inline glm::vec4 GetVec4() const
		{
			return glm::vec4(static_cast<float>(R) / 255.0f, static_cast<float>(G) / 255.0f, static_cast<float>(B) / 255.0f, static_cast<float>(A) / 255.0f);
		}

		operator uint32_t() const { return R | G << 8 | B << 16 | A << 24; }

		uint8_t R;
		uint8_t G;
		uint8_t B;
		uint8_t A;
	};
}