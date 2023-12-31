#include "PostProcessing.hpp"
#include "Passes/TAAPass.hpp"
#include "Passes/TonemapperPass.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	PostProcessing::PostProcessing()
		: m_taaPass(nullptr)
		, m_tonemapperPass(nullptr)
		, m_taaFrameIndex(0)
		, m_taaJitterOffsets()
	{
	}

	bool PostProcessing::Initialise()
	{
		m_taaPass = std::make_unique<TAAPass>();
		m_tonemapperPass = std::make_unique<TonemapperPass>();
		return true;
	}

	float Halton(size_t i, uint32_t b)
	{
		float f = 1.0f;
		float r = 0.0f;

		while (i > 0)
		{
			f /= static_cast<float>(b);
			r = r + f * static_cast<float>(i % b);
			i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
		}

		return r;
	}

	bool PostProcessing::Rebuild(const glm::uvec2& size)
	{
		// Populate TAA jitter offsets with quasi-random sequence.
		for (size_t i = 0; i < m_taaJitterOffsets.size(); ++i)
		{
			m_taaJitterOffsets[i].x = (2.0f * Halton(i + 1, 2) - 1.0f) / size.x;
			m_taaJitterOffsets[i].y = (2.0f * Halton(i + 1, 3) - 1.0f) / size.y;
		}

		return true;
	}
}