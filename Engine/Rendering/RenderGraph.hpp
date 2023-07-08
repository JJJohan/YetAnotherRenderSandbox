#pragma once

#include <vector>
#include <unordered_map>

#include "Passes/IRenderPass.hpp"

namespace Engine::Rendering
{
	class Renderer;
	class IMaterialManager;

	struct RenderNode
	{
		std::unordered_map<const char*, const IRenderResource*> InputBuffers;
		std::unordered_map<const char*, const IRenderResource*> InputImages;
		IRenderPass* Pass;
		IRenderResource* Resource;

		RenderNode(IRenderResource* resource)
			: Pass(nullptr)
			, Resource(resource)
			, InputBuffers()
			, InputImages()
		{
		}

		RenderNode(IRenderPass* pass)
			: Pass(pass)
			, Resource(pass)
			, InputBuffers()
			, InputImages()
		{
		}
	};

	class RenderGraph
	{
	public:
		RenderGraph();

		bool AddPass(IRenderPass* renderPass, const IMaterialManager& materialManager);
		bool AddResource(IRenderResource* renderResource);
		bool Build(const Renderer& renderer);
		void Draw(const Renderer& renderer) const;

		inline const std::unordered_map<const char*, IRenderPass*>& GetPasses() const { return m_renderPasses; }
		inline const std::vector<std::vector<RenderNode>>& GetBuiltGraph() const { return m_renderGraph; }
		inline void MarkDirty() { m_dirty = true; }
		inline bool CheckDirty() const { return m_dirty; }
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
		bool m_dirty;
		std::unordered_map<const char*, IRenderPass*> m_renderPasses;
		std::unordered_map<const char*, IRenderResource*> m_renderResources;
		std::vector<std::vector<RenderNode>> m_renderGraph;
		std::unordered_map<const char*, const IRenderResource*> m_imageResourceNodeLookup;
		std::unordered_map<const char*, const IRenderResource*> m_bufferResourceNodeLookup;
	};
}