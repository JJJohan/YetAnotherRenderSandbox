#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "Passes/IRenderPass.hpp"
#include "RenderResources/IRenderResource.hpp"

namespace Engine::Rendering
{
	class Renderer;
	class IMaterialManager;
	class ICommandPool;
	class IResourceFactory;
	class ISemaphore;
	class RenderStats;
	class IRenderImage;

	struct RenderGraphNode
	{
		std::unordered_map<const char*, RenderGraphNode&> InputBufferSources;
		std::unordered_map<const char*, RenderGraphNode&> InputImageSources;
		std::unordered_map<const char*, IRenderImage*> InputImages;
		std::unordered_map<const char*, IRenderImage*> OutputImages;
		IRenderPass* Pass;
		IRenderResource* Resource;
		IRenderNode* Node;
		RenderNodeType Type;

		RenderGraphNode(IRenderNode* node)
			: Type(node->GetNodeType())
			, Node(node)
			, InputBufferSources()
			, InputImageSources()
			, InputImages()
			, OutputImages()
		{
			if (Type == RenderNodeType::Pass)
			{
				Pass = static_cast<IRenderPass*>(node);
				Resource = nullptr;
			}
			else
			{
				Pass = nullptr;
				Resource = static_cast<IRenderResource*>(node);
			}
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

		inline const std::vector<std::vector<RenderGraphNode>>& GetBuiltGraph() const { return m_renderGraph; }
		inline void MarkDirty() { m_dirty = true; }
		inline bool CheckDirty() const { return m_dirty; }
		inline bool TryGetPass(const char* name, const IRenderPass** result) const
		{
			const auto& search = m_renderNodeLookup.find(name);
			if (search != m_renderNodeLookup.end() && search->second->GetNodeType() == RenderNodeType::Pass)
			{
				*result = static_cast<const IRenderPass*>(search->second);
				return true;
			}

			return false;
		}

	private:
		bool m_dirty;
		const RenderGraphNode* m_finalNode;
		RenderStats& m_renderStats;

		std::unordered_map<const char*, IRenderNode*> m_renderNodeLookup;
		std::vector<IRenderPass*> m_renderPasses;
		std::vector<IRenderResource*> m_renderResources;
		std::vector<std::vector<RenderGraphNode>> m_renderGraph;

		std::vector<std::unique_ptr<ICommandBuffer>> m_blitCommandBuffers;
		std::unordered_map<IRenderPass*, std::vector<std::unique_ptr<ICommandBuffer>>> m_commandBuffers;
		std::unique_ptr<ICommandPool> m_commandPool;
		std::unique_ptr<ISemaphore> m_semaphore;

		std::unordered_map<const char*, const IRenderNode*> m_imageResourceNodeLookup;
		std::unordered_map<const char*, const IRenderNode*> m_bufferResourceNodeLookup;
		std::vector<std::unique_ptr<IRenderImage>> m_renderTextures;
	};
}