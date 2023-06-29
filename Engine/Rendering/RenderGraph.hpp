#pragma once

#include <vector>
#include <unordered_map>

namespace Engine::Rendering
{
	class IRenderPass;
	class Renderer;

	struct RenderNode
	{
		std::unordered_map<const char*, const IRenderPass*> InputBuffers;
		std::unordered_map<const char*, const IRenderPass*> InputImages;
		const IRenderPass* Pass;

		RenderNode(const IRenderPass* pass)
			: Pass(pass)
			, InputBuffers()
			, InputImages()
		{
		}
	};

	class RenderGraph
	{
	public:
		RenderGraph();

		bool AddPass(const IRenderPass* renderPass);
		bool Build();
		void Draw(const Renderer& renderer) const;

		inline const std::unordered_map<const char*, const IRenderPass*>& GetPasses() const { return m_renderPasses; }
		inline const std::vector<std::vector<RenderNode>>& GetBuiltGraph() const { return m_renderGraph; }
		inline bool TryGetPass(const char* name, const IRenderPass** result) const
		{
			const auto& search = m_renderPasses.find(name);
			if (search != m_renderPasses.end())
			{
				*result = search->second;
				return true;
			}

			return false;
		}

	private:
		std::unordered_map<const char*, const IRenderPass*> m_renderPasses;
		std::vector<std::vector<RenderNode>> m_renderGraph;
		std::unordered_map<const char*, const IRenderPass*> m_imageResourceNodeLookup;
		std::unordered_map<const char*, const IRenderPass*> m_bufferResourceNodeLookup;
	};
}