#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Engine::Rendering
{
	class IRenderPass;

	class PostProcessing
	{
	public:
		PostProcessing();

		bool Initialise();
		bool Rebuild(const glm::uvec2& size);

		inline std::vector<IRenderPass*> GetRenderPasses() const
		{
			return { m_tonemapperPass.get(), m_fxaaPass.get(), m_smaaEdgesPass.get(),
				m_smaaWeightsPass.get(), m_smaaBlendPass.get(), m_taaPass.get() };
		}

		inline const glm::vec2& GetTAAJitter()
		{
			return m_taaJitterOffsets[++m_taaFrameIndex % m_taaJitterOffsets.size()];
		}

	private:

		uint32_t m_taaFrameIndex;
		std::array<glm::vec2, 6> m_taaJitterOffsets;
		std::unique_ptr<IRenderPass> m_fxaaPass;
		std::unique_ptr<IRenderPass> m_smaaEdgesPass;
		std::unique_ptr<IRenderPass> m_smaaWeightsPass;
		std::unique_ptr<IRenderPass> m_smaaBlendPass;
		std::unique_ptr<IRenderPass> m_taaPass;
		std::unique_ptr<IRenderPass> m_tonemapperPass;
	};
}