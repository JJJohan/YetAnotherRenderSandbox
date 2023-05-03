#pragma once

#include <stdint.h>
#include "Macros.hpp"

namespace Engine
{
	class EXPORT Colour
	{
	public:
		Colour(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

		bool operator==(const Colour& other) const
		{
			return R == other.R && G == other.G && B == other.B && A == other.A;
		}

		uint8_t R;
		uint8_t G;
		uint8_t B;
		uint8_t A;
	};
}