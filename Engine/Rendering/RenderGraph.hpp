#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "RenderPasses/IRenderPass.hpp"
#include "ComputePasses/IComputePass.hpp"
#include "RenderResources/IRenderResource.hpp"
#include "RenderResources/RenderPassResourceInfo.hpp"
#include "Resources/SubmitInfo.hpp"

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
			const IResourceFactory& resourceFactory, uint32_t concurrentFrameCount, bool asyncCompute);

		bool AddRenderNode(IRenderNode* renderNode, const IMaterialManager& materialManager);
		bool AddResource(IRenderResource* renderResource);
		bool Build(const Renderer& renderer, bool asyncCompute);
		bool Draw(Renderer& renderer, uint32_t frameIndex);
		void SetPassEnabled(const std::string& passName, bool enabled);

		inline const std::vector<std::vector<RenderGraphNode>>& GetBuiltGraph() const { return m_renderGraph; }
		inline void MarkDirty() { m_dirty = true; }
		inline bool CheckDirty() const { return m_dirty; }

		inline bool TryGetRenderPass(const std::string& name, const IRenderPass** result) const
		{
			const auto& search = m_renderNodeLookup.find(name);
			if (search != m_renderNodeLookup.end() && search->second->GetNodeType() == RenderNodeType::Pass)
			{
				if (result)
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
				if (result)
					*result = static_cast<const IComputePass*>(search->second);
				return true;
			}

			return false;
		}

	private:
		struct ImageInfo
		{
			AccessFlags Access;
			IRenderImage& Image;

			ImageInfo(IRenderImage& image)
				: Image(image)
				, Access(AccessFlags::None)
			{
			}
		};

		inline bool TryGetOrAddImage(std::string_view name, const Renderer& renderer, std::unordered_map<Format, std::vector<ImageInfo>>& formatRenderTextureLookup,
			std::vector<std::unique_ptr<IRenderImage>>& renderTextures, std::unordered_map<IRenderImage*, uint32_t>& imageInfoLookup,
			Format format, AccessFlags accessFlags, const glm::uvec3& dimensions, IRenderImage** result) const;

		bool DetermineRequiredResources(std::vector<IRenderNode*>& renderNodeStack,
			std::unordered_map<std::string, RenderGraphNode*>& availableImageSources,
			std::unordered_map<std::string, RenderGraphNode*>& availableBufferSources);

		bool ReserveRenderTexturesForPasses(const Renderer& renderer, const glm::uvec3& defaultExtents,
			std::unordered_map<Format, std::vector<ImageInfo>>& formatRenderTextureLookup,
			std::unordered_map<IRenderImage*, uint32_t>& imageInfoLookup);

		bool BuildPasses(const Renderer& renderer, const IDevice& device);

		bool FindFinalNode();

		void TransitionResourcesForStage(const Renderer& renderer, const ICommandBuffer& commandBuffer, bool isCompute,
			const std::vector<RenderGraphNode>& nodes);

		void ReleaseResourceQueueFamilyOwnership(Renderer& renderer, const ICommandBuffer& renderCommandBuffer,
			const ICommandBuffer& computeCommandBuffer, const std::vector<std::pair<std::string, bool>>& resourcesToTransfer,
			bool& renderToCompute, bool& computeToRender);

		bool DrawRenderPass(const Renderer& renderer, const RenderGraphNode& node,
			const ICommandBuffer& commandBuffer, uint32_t frameIndex, const glm::uvec2& size) const;

		bool DispatchComputePass(Renderer& renderer, const RenderGraphNode& node, const ICommandBuffer& commandBuffer,
			uint32_t frameIndex) const;

		bool BlitToSwapchain(Renderer& renderer, const IDevice& device, uint32_t frameIndex,
			std::vector<SubmitInfo>& renderSubmitInfos, std::vector<SubmitInfo>& computeSubmitInfos) const;

		bool m_dirty;
		bool m_asyncCompute;
		const RenderGraphNode* m_finalNode;
		RenderStats& m_renderStats;

		std::unordered_map<std::string, IRenderNode*> m_renderNodeLookup;
		std::vector<IRenderPass*> m_renderPasses;
		std::vector<IComputePass*> m_computePasses;
		std::vector<IRenderResource*> m_renderResources;
		std::vector<std::vector<RenderGraphNode>> m_renderGraph;

		std::vector<std::unique_ptr<ICommandBuffer>> m_blitCommandBuffers;
		std::vector<std::vector<std::unique_ptr<ICommandBuffer>>> m_renderCommandBuffers;
		std::vector<std::vector<std::unique_ptr<ICommandBuffer>>> m_computeCommandBuffers;
		std::vector<std::unique_ptr<ICommandPool>> m_renderCommandPools;
		std::vector<std::unique_ptr<ICommandPool>> m_computeCommandPools;
		std::unique_ptr<ISemaphore> m_renderToComputeSemaphore;
		std::unique_ptr<ISemaphore> m_computeToRenderSemaphore;
		std::unordered_map<std::string, RenderPassBufferInfo> m_bufferInfoBarrierState;
		std::unordered_map<std::string, RenderPassImageInfo> m_imageInfoBarrierState;
		std::vector<std::vector<std::pair<std::string, bool>>> m_perStageOwnershipReleaseResources;

		std::unordered_map<std::string, const IRenderNode*> m_imageResourceNodeLookup;
		std::unordered_map<std::string, const IRenderNode*> m_bufferResourceNodeLookup;
		std::vector<std::unique_ptr<IRenderImage>> m_renderTextures;
	};
}