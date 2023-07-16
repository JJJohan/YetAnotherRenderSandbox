#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "Passes/IRenderPass.hpp"

namespace Engine::Rendering
{
	class Renderer;
	class IMaterialManager;
	class ICommandPool;
	class IResourceFactory;
	class ISemaphore;
	class RenderStats;

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
		RenderGraph(RenderStats& renderStats);
		virtual ~RenderGraph();

		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
			const IResourceFactory& resourceFactory, uint32_t concurrentFrameCount);

		bool AddPass(IRenderPass* renderPass, const IMaterialManager& materialManager);
		bool AddResource(IRenderResource* renderResource);
		bool Build(const Renderer& renderer);
		bool Draw(Renderer& renderer, uint32_t frameIndex) const;
		void SetPassEnabled(const char* passName, bool enabled);

		inline const std::vector<std::vector<RenderNode>>& GetBuiltGraph() const { return m_renderGraph; }
		inline void MarkDirty() { m_dirty = true; }
		inline bool CheckDirty() const { return m_dirty; }
		inline bool TryGetPass(const char* name, const IRenderPass** result) const
		{
			const auto& search = m_renderPassLookup.find(name);
			if (search != m_renderPassLookup.end())
			{
				*result = search->second;
				return true;
			}

			return false;
		}

	private:
		bool m_dirty;
		IRenderPass* m_finalPass;
		IRenderImage* m_finalImage;
		std::unordered_map<const char*, IRenderPass*> m_renderPassLookup;
		std::vector<IRenderPass*> m_renderPasses;
		std::unordered_map<const char*, IRenderResource*> m_renderResources;
		std::vector<std::unique_ptr<ICommandBuffer>> m_blitCommandBuffers;
		std::unordered_map<IRenderPass*, std::vector<std::unique_ptr<ICommandBuffer>>> m_commandBuffers;
		std::unique_ptr<ICommandPool> m_commandPool;
		std::unique_ptr<ISemaphore> m_semaphore;
		std::vector<std::vector<RenderNode>> m_renderGraph;
		std::unordered_map<const char*, const IRenderResource*> m_imageResourceNodeLookup;
		std::unordered_map<const char*, const IRenderResource*> m_bufferResourceNodeLookup;
		RenderStats& m_renderStats;
	};
}