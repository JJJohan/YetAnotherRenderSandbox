#pragma once

namespace Engine::Rendering
{
	struct RenderSettings
	{
		uint32_t m_multiSampleCount;
		bool m_temporalAA;
		bool m_hdr;

		RenderSettings()
			: m_multiSampleCount(1)
			, m_temporalAA(true)
			, m_hdr(false)
		{
		}
	};
}