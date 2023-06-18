#pragma once

#include <Core/Colour.hpp>

namespace Sandbox
{
	struct Options
	{
		Options()
			: ClearColour(0, 0, 0)
			, SunColour(1, 1, 0.9f)
			, SunIntensity(5.0f)
			, UseTAA(true)
			, UseHDR(false)
		{
		}

		Engine::Colour ClearColour;
		Engine::Colour SunColour;
		float SunIntensity;
		bool UseTAA;
		bool UseHDR;
	};
}