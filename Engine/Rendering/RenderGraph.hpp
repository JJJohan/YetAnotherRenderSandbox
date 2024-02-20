#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "RenderPasses/IRenderPass.hpp"
#include "ComputePasses/IComputePass.hpp"
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
		std::unordered_map<std::string, RenderGraphNode&> InputBufferSources;
		std::unordered_map<std::string, RenderGraphNode&> InputImageSources;
		std::unordered_map<std::string, IRenderImage*> InputImages;
		std::unordered_map<std::string, IRenderImage*> OutputImages;
		std::unordered_map<std::string, IBuffer*> InputBuffers;
		std::unordered_map<std::string, IBuffer*> OutputBuffers;
		IRenderNode* Node;
		RenderNodeType Type;

		RenderGraphNode(IRenderNode* node)
			: Type(node->GetNodeType())
			, Node(node)
			, InputBufferSources()
			, InputImageSources()
			, InputImages()
			, OutputImages()
			, InputBuffers()
			, OutputBuffers()
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

		bool AddRenderNode(IRenderNode* renderNode, const IMaterialManager& materialManager);
		bool AddResource(IRenderResource* renderResource);
		bool Build(const Renderer& renderer);
		bool Draw(Renderer& renderer, uint32_t frameIndex) const;
		void SetPassEnabled(const std::string& passName, bool enabled);

		inline const std::vector<std::vector<RenderGraphNode>>& GetBuiltGraph() const { return m_renderGraph; }
		inline void MarkDirty() { m_dirty = true; }
		inline bool CheckDirty() const { return m_dirty; }

		inline bool TryGetRenderPass(const std::string& name, const IRenderPass** result) const
		{
			const auto& search = m_renderNodeLookup.find(name);
			if (search != m_renderNodeLookup.end() && search->second->GetNodeType() == RenderNodeType::Pass)
			{
				*result = static_cast<const IRenderPass*>(search->second);
				return true;
			}

			return false;
		}

		inline bool TryGetComputePass(const std::string& name, const IComputePass** result) const
		{
			const auto& search = m_renderNodeLookup.find(name);
			if (search != m_renderNodeLookup.end() && search->second->GetNodeType() == RenderNodeType::Compute)
			{
				*result = static_cast<const IComputePass*>(search->second);
				return true;
			}

			return false;
		}

	private:
		bool m_dirty;
		const RenderGraphNode* m_finalNode;
		RenderStats& m_renderStats;

		std::unordered_map<std::string, IRenderNode*> m_renderNodeLookup;
		std::vector<IRenderPass*> m_renderPasses;
		std::vector<IComputePass*> m_computePasses;
		std::vector<IRenderResource*> m_renderResources;
		std::vector<std::vector<RenderGraphNode>> m_renderGraph;

		std::vector<std::unique_ptr<ICommandBuffer>> m_blitCommandBuffers;
		std::unordered_map<IRenderPass*, std::vector<std::unique_ptr<ICommandBuffer>>> m_renderCommandBuffers;
		std::unordered_map<IComputePass*, std::vector<std::unique_ptr<ICommandBuffer>>> m_computeCommandBuffers;
		std::unique_ptr<ICommandPool> m_renderCommandPool;
		std::unique_ptr<ICommandPool> m_computeCommandPool;
		std::unique_ptr<ISemaphore> m_renderSemaphore;
		std::unique_ptr<ISemaphore> m_computeSemaphore;

		std::unordered_map<std::string, const IRenderNode*> m_imageResourceNodeLookup;
		std::unordered_map<std::string, const IRenderNode*> m_bufferResourceNodeLookup;
		std::vector<std::unique_ptr<IRenderImage>> m_renderTextures;
	};
}