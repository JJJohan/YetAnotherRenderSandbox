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
			return { m_taaPass.get() };
		}

		inline const glm::vec2& GetTAAJitter()
		{
			return m_taaJitterOffsets[++m_taaFrameIndex % m_taaJitterOffsets.size()];
		}

		inline void SetEnabled(bool enabled) { m_enabled = enabled; };
		inline bool IsEnabled() { return m_enabled; };

	private:
		bool m_enabled;

		uint32_t m_taaFrameIndex;
		std::array<glm::vec2, 6> m_taaJitterOffsets;
		std::unique_ptr<IRenderPass> m_taaPass;
	};
}