#pragma once

#include <Core/Colour.hpp>
#include <Rendering/CullingMode.hpp>
#include <Rendering/AntiAliasingMode.hpp>

namespace Sandbox
{
	struct Options
	{
		Options()
			: ClearColour(0, 0, 0)
			, SunColour(1, 1, 0.9f)
			, SunIntensity(5.0f)
			, AntiAliasingMode(Engine::Rendering::AntiAliasingMode::TAA)
			, UseHDR(false)
			, CullingMode(Engine::Rendering::CullingMode::FrustumAndOcclusion)
			, ShadowResolutionIndex(2)
		{
		}

		Engine::Colour ClearColour;
		Engine::Colour SunColour;
		float SunIntensity;
		Engine::Rendering::AntiAliasingMode AntiAliasingMode;
		bool UseHDR;
		Engine::Rendering::CullingMode CullingMode;
		int32_t ShadowResolutionIndex;
	};
}