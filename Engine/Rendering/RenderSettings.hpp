#pragma once

#include "AntiAliasingMode.hpp"

namespace Engine::Rendering
{
	struct RenderSettings
	{
		uint32_t m_multiSampleCount;
		AntiAliasingMode m_aaMode;
		bool m_hdr;

		RenderSettings()
			: m_multiSampleCount(1)
			, m_aaMode(AntiAliasingMode::TAA)
			, m_hdr(false)
		{
		}
	};
}