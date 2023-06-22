#pragma once

#include <vector>
#include <unordered_map>

namespace Engine::Rendering
{
	class IRenderPass;
	class Renderer;
	class IBuffer;
	class IRenderImage;

	class RenderGraph
	{
	public:
		RenderGraph();

		bool AddPass(const IRenderPass* renderPass);
		bool Build();
		void Draw(const Renderer& renderer) const;

	private:
		std::vector<const IRenderPass*> m_renderPasses;
		std::vector<std::vector<const IRenderPass*>> m_renderGraph;
		std::unordered_map<const char*, const IRenderPass*> m_bufferResourceMap;
		std::unordered_map<const char*, const IRenderPass*> m_imageResourceMap;
	};
}