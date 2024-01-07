#pragma once

#include <Core/Colour.hpp>
#include <Rendering/CullingMode.hpp>

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
			, CullingMode(Engine::Rendering::CullingMode::FrustumAndOcclusion)
		{
		}

		Engine::Colour ClearColour;
		Engine::Colour SunColour;
		float SunIntensity;
		bool UseTAA;
		bool UseHDR;
		Engine::Rendering::CullingMode CullingMode;
	};
}