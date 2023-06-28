#pragma once

#include <vector>
#include <unordered_map>

namespace Engine::Rendering
{
	class IRenderPass;
	class Renderer;

	class RenderGraph
	{
	public:
		RenderGraph();

		bool AddPass(const IRenderPass* renderPass);
		bool Build();
		void Draw(const Renderer& renderer) const;

	private:
		struct RenderNode
		{
			std::unordered_map<const char*, const IRenderPass*> InputBuffers;
			std::unordered_map<const char*, const IRenderPass*> InputImages;
			const IRenderPass* Pass;

			RenderNode(const IRenderPass* pass) 
				: Pass(pass)
				, InputBuffers()
				, InputImages()
			{}
		};

		std::vector<const IRenderPass*> m_renderPasses;
		std::vector<std::vector<RenderNode>> m_renderGraph;
		std::unordered_map<const char*, const IRenderPass*> m_imageResourceNodeLookup;
		std::unordered_map<const char*, const IRenderPass*> m_bufferResourceNodeLookup;
	};
}