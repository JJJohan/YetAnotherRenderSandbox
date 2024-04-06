#include "PostProcessing.hpp"
#include "RenderPasses/PostProcessing/FXAAPass.hpp"
#include "RenderPasses/PostProcessing/SMAAEdgesPass.hpp"
#include "RenderPasses/PostProcessing/SMAAWeightsPass.hpp"
#include "RenderPasses/PostProcessing/SMAABlendPass.hpp"
#include "RenderPasses/PostProcessing/TAAPass.hpp"
#include "RenderPasses/PostProcessing/TonemapperPass.hpp"

using namespace Engine::Logging;

namespace Engine::Rendering
{
	PostProcessing::PostProcessing()
		: m_fxaaPass(nullptr)
		, m_smaaEdgesPass(nullptr)
		, m_smaaWeightsPass(nullptr)
		, m_smaaBlendPass(nullptr)
		, m_taaPass(nullptr)
		, m_tonemapperPass(nullptr)
		, m_taaFrameIndex(0)
		, m_taaJitterOffsets()
	{
	}

	bool PostProcessing::Initialise()
	{
		m_fxaaPass = std::make_unique<FXAAPass>();
		m_smaaEdgesPass = std::make_unique<SMAAEdgesPass>();
		m_smaaWeightsPass = std::make_unique<SMAAWeightsPass>();
		m_smaaBlendPass = std::make_unique<SMAABlendPass>();
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