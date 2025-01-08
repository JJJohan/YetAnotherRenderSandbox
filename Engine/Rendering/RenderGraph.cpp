#include "RenderGraph.hpp"
#include "Renderer.hpp"
#include "Core/Logger.hpp"
#include "IMaterialManager.hpp"
#include "QueueFamilyIndices.hpp"
#include "Resources/ICommandBuffer.hpp"
#include "Resources/ICommandPool.hpp"
#include "Resources/ISemaphore.hpp"
#include "Resources/IBuffer.hpp"
#include "Resources/IMemoryBarriers.hpp"
#include "IResourceFactory.hpp"
#include "IPhysicalDevice.hpp"
#include <ranges>

namespace Engine::Rendering
{
	RenderGraph::RenderGraph(RenderStats& renderStats)
		: m_bufferResourceNodeLookup()
		, m_imageResourceNodeLookup()
		, m_renderGraph()
		, m_renderPasses()
		, m_renderNodeLookup()
		, m_dirty(true)
		, m_asyncCompute(false)
		, m_finalNode(nullptr)
		, m_renderCommandBuffers()
		, m_computeCommandBuffers()
		, m_renderCommandPools()
		, m_computeCommandPools()
		, m_imageInfoBarrierState()
		, m_bufferInfoBarrierState()
		, m_renderToComputeSemaphore(nullptr)
		, m_computeToRenderSemaphore(nullptr)
		, m_renderTextures()
		, m_renderStats(renderStats)
		, m_blitCommandBuffers()
		, m_perStageOwnershipReleaseResources()
	{
	}

	RenderGraph::~RenderGraph()
	{
		m_blitCommandBuffers.clear();
		m_renderCommandBuffers.clear();
		m_computeCommandBuffers.clear();
		m_renderCommandPools.clear();
		m_computeCommandPools.clear();
	}

	bool RenderGraph::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device,
		const IResourceFactory& resourceFactory, uint32_t concurrentFrameCount, bool asyncCompute)
	{
		const QueueFamilyIndices& indices = physicalDevice.GetQueueFamilyIndices();

		m_renderToComputeSemaphore.reset();
		m_computeToRenderSemaphore.reset();
		m_renderCommandBuffers.clear();
		m_computeCommandBuffers.clear();
		m_blitCommandBuffers.clear();
		m_renderCommandPools.clear();
		m_computeCommandPools.clear();

		if (asyncCompute)
		{
			m_renderToComputeSemaphore = std::move(resourceFactory.CreateGraphicsSemaphore());
			if (!m_renderToComputeSemaphore->Initialise("GraphicsToComputeSemaphore", device))
			{
				return false;
			}

			m_computeToRenderSemaphore = std::move(resourceFactory.CreateGraphicsSemaphore());
			if (!m_computeToRenderSemaphore->Initialise("ComputeToGraphicsSemaphore", device))
			{
				return false;
			}
		}

		for (uint32_t i = 0; i < concurrentFrameCount; ++i)
		{
			m_renderCommandPools.emplace_back(std::move(resourceFactory.CreateCommandPool()));
			if (!m_renderCommandPools[i]->Initialise("RenderCommandPool", physicalDevice, device, indices.GraphicsFamily.value(), CommandPoolFlags::None))
			{
				return false;
			}

			auto commandBuffers = m_renderCommandPools[i]->CreateCommandBuffers("BlitCommandBuffer", device, 1);
			if (commandBuffers.empty())
			{
				return false;
			}
			m_blitCommandBuffers.emplace_back(std::move(commandBuffers[0]));

			if (asyncCompute)
			{
				m_computeCommandPools.emplace_back(std::move(resourceFactory.CreateCommandPool()));
				if (!m_computeCommandPools[i]->Initialise("ComputeCommandPool", physicalDevice, device, indices.ComputeFamily.value(), CommandPoolFlags::None))
				{
					return false;
				}
			}
		}

		m_asyncCompute = asyncCompute;

		return true;
	}

	bool RenderGraph::AddRenderNode(IRenderNode* renderNode, const IMaterialManager& materialManager)
	{
		if (m_renderNodeLookup.contains(renderNode->GetName()))
		{
			Logger::Error("Render node with name '{}' already exists in the render graph.", renderNode->GetName());
			return false;
		}

		std::unordered_map<std::string, RenderPassBufferInfo> inputBufferInfos;
		std::unordered_map<std::string, RenderPassBufferInfo> outputBufferInfos;

		if (renderNode->GetNodeType() == RenderNodeType::Pass)
		{
			IRenderPass* renderPass = static_cast<IRenderPass*>(renderNode);
			inputBufferInfos = renderPass->GetBufferInputInfos();
			outputBufferInfos = renderPass->GetBufferOutputInfos();
			std::unordered_map<std::string, RenderPassImageInfo> inputImageInfos(renderPass->GetImageInputInfos().begin(), renderPass->GetImageInputInfos().end());

			for (const auto& imageOutput : renderPass->GetImageOutputInfos())
			{
				if (m_imageResourceNodeLookup.contains(imageOutput.first))
				{
					if (inputImageInfos.contains(imageOutput.first))
					{
						// Pass-through resource.
						continue;
					}

					Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", imageOutput.first);
					return false;
				}

				m_imageResourceNodeLookup[imageOutput.first] = renderPass;
			}

			if (!renderPass->Initialise(materialManager))
				return false;

			m_renderPasses.emplace_back(renderPass);
		}
		else if (renderNode->GetNodeType() == RenderNodeType::Compute)
		{
			IComputePass* computePass = static_cast<IComputePass*>(renderNode);
			inputBufferInfos = computePass->GetBufferInputInfos();
			outputBufferInfos = computePass->GetBufferOutputInfos();

			if (!computePass->Initialise(materialManager))
				return false;

			m_computePasses.emplace_back(computePass);
		}
		else
		{
			Logger::Error("Render graph only supports render passes and compute passes.");
			return false;
		}

		for (const auto& bufferOutput : outputBufferInfos)
		{
			if (m_bufferResourceNodeLookup.contains(bufferOutput.first))
			{
				if (inputBufferInfos.contains(bufferOutput.first))
				{
					// Pass-through resource.
					continue;
				}

				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", bufferOutput.first);
				return false;
			}

			m_bufferResourceNodeLookup[bufferOutput.first] = renderNode;
		}

		m_renderNodeLookup[renderNode->GetName()] = renderNode;
		return true;
	}

	void RenderGraph::SetPassEnabled(const std::string& passName, bool enabled)
	{
		auto pass = m_renderNodeLookup.find(passName);
		if (pass == m_renderNodeLookup.end())
		{
			Logger::Error("Pass '{}' not found in render graph.", passName);
			return;
		}

		pass->second->SetEnabled(enabled);
		m_dirty = true;
	}

	bool RenderGraph::GetPassEnabled(const std::string& passName) const
	{
		auto pass = m_renderNodeLookup.find(passName);
		if (pass == m_renderNodeLookup.end())
		{
			Logger::Error("Pass '{}' not found in render graph.", passName);
			return false;
		}

		return pass->second->GetEnabled();
	}

	bool RenderGraph::AddResource(IRenderResource* renderResource)
	{
		if (m_renderNodeLookup.contains(renderResource->GetName()))
		{
			Logger::Error("Render resource with name '{}' already exists in the render graph.", renderResource->GetName());
			return false;
		}

		for (const auto bufferOutput : renderResource->GetBufferOutputInfos())
		{
			if (m_bufferResourceNodeLookup.contains(bufferOutput.first))
			{
				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", bufferOutput.first);
				return false;
			}

			m_bufferResourceNodeLookup[bufferOutput.first] = renderResource;
		}

		for (const auto& imageOutput : renderResource->GetImageOutputInfos())
		{
			if (m_imageResourceNodeLookup.contains(imageOutput.first))
			{
				Logger::Error("Attempted to add a resource with the name '{}' which already exists in the render graph.", imageOutput.first);
				return false;
			}

			m_imageResourceNodeLookup[imageOutput.first] = renderResource;
		}

		m_renderResources.emplace_back(renderResource);
		m_renderNodeLookup[renderResource->GetName()] = renderResource;
		return true;
	}

	inline bool RenderGraph::TryGetOrAddImage(std::string_view name, const Renderer& renderer, std::unordered_map<Format, std::vector<ImageInfo>>& formatRenderTextureLookup,
		std::vector<std::unique_ptr<IRenderImage>>& renderTextures, std::unordered_map<IRenderImage*, uint32_t>& imageInfoLookup,
		Format format, AccessFlags accessFlags, const glm::uvec3& dimensions, IRenderImage** result) const
	{
		if (format == Format::PlaceholderDepth || format == Format::PlaceholderSwapchain)
		{
			Logger::Error("Placeholder format should be handled by IRenderNode::UpdatePlaceholderFormats.");
			return false;
		}

		const IPhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();
		Format depthFormat = physicalDevice.GetDepthFormat();

		std::vector<ImageInfo>& availableImages = formatRenderTextureLookup[format];

		for (auto& info : availableImages)
		{
			// Ensure requested dimensions match.
			if (info.Image.GetDimensions() != dimensions)
				continue;

			// Multiple reads are allowed.
			if (accessFlags == AccessFlags::Read && info.Access != AccessFlags::Write)
			{
				info.Access = AccessFlags::Read;
				*result = &info.Image;
				return true;
			}

			// Allow write if no existing reads or writes exist.
			if (accessFlags == AccessFlags::Write && info.Access == AccessFlags::None)
			{
				info.Access = AccessFlags::Write;
				*result = &info.Image;
				return true;
			}

			// Passthrough (e.g. directly received from a render resource.)
			if (accessFlags == AccessFlags::None)
			{
				*result = &info.Image;
				return true;
			}
		}

		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		const IDevice& device = renderer.GetDevice();
		const ISwapChain& swapchain = renderer.GetSwapChain();

		std::unique_ptr<IRenderImage>& image = renderTextures.emplace_back(std::move(resourceFactory.CreateRenderImage()));

		ImageAspectFlags aspectFlags;
		ImageUsageFlags usageFlags;
		if (format == depthFormat)
		{
			aspectFlags = ImageAspectFlags::Depth;
			usageFlags = ImageUsageFlags::DepthStencilAttachment | ImageUsageFlags::Sampled | ImageUsageFlags::TransferDst;
		}
		else
		{
			aspectFlags = ImageAspectFlags::Color;
			usageFlags = ImageUsageFlags::ColorAttachment | ImageUsageFlags::Sampled | ImageUsageFlags::TransferSrc | ImageUsageFlags::TransferDst;
		}

		if (!image->Initialise(name, device, ImageType::e2D, format, dimensions, 1, 1, ImageTiling::Optimal,
			usageFlags, aspectFlags, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			*result = nullptr;
			return false;
		}

		imageInfoLookup.emplace(image.get(), static_cast<uint32_t>(availableImages.size()));
		ImageInfo& newInfo = availableImages.emplace_back(ImageInfo(*image));
		newInfo.Access = accessFlags;

		*result = image.get();

		return true;
	}

	bool RenderGraph::DetermineRequiredResources(std::vector<IRenderNode*>& renderNodeStack,
		std::unordered_map<std::string, RenderGraphNode*>& availableImageSources,
		std::unordered_map<std::string, RenderGraphNode*>& availableBufferSources)
	{
		while (!renderNodeStack.empty())
		{
			std::vector<RenderGraphNode>& stage = m_renderGraph.emplace_back(std::vector<RenderGraphNode>());

			// Per-stage available resources.
			std::unordered_map<std::string, RenderGraphNode*> stageAvailableBufferSources(availableBufferSources);
			std::unordered_map<std::string, RenderGraphNode*> stageAvailableImageSources(availableImageSources);

			// Attempt to satisfy all requirements for each remaining render pass. If all requirements are met for a pass, remove it from the stack.
			for (auto it = renderNodeStack.begin(); it != renderNodeStack.end();)
			{
				IRenderNode* renderNode = *it;
				RenderGraphNode node = RenderGraphNode(renderNode);
				bool satisfied = true;

				for (const auto& bufferInput : node.Node->GetBufferInputInfos())
				{
					const auto& search = stageAvailableBufferSources.find(bufferInput.first);
					if (search != stageAvailableBufferSources.end())
					{
						// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
						node.InputBufferSources.emplace(bufferInput.first, *search->second);
						for (const auto& bufferOutput : node.Node->GetBufferOutputInfos())
						{
							if (bufferOutput.first == search->first)
							{
								stageAvailableBufferSources.erase(bufferOutput.first);
								break;
							}
						}
					}
					else
					{
						satisfied = false;
						break;
					}
				}

				if (satisfied)
				{
					for (const auto& imageInputInfo : node.Node->GetImageInputInfos())
					{
						const auto& search = stageAvailableImageSources.find(imageInputInfo.first);
						if (search != stageAvailableImageSources.end())
						{
							// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
							node.InputImageSources.insert_or_assign(imageInputInfo.first, *search->second);
							for (const auto& imageOutput : node.Node->GetImageOutputInfos())
							{
								if (imageOutput.first == search->first)
								{
									stageAvailableImageSources.erase(imageOutput.first);
									break;
								}
							}
						}
						else
						{
							satisfied = false;
							break;
						}
					}
				}

				if (satisfied)
				{
					stage.emplace_back(std::move(node));
					it = renderNodeStack.erase(it);
				}
				else
				{
					++it;
				}
			}

			if (stage.empty())
			{
				Logger::Error("Could not resolve requirements for remaining render passes while building render graph.");
				return false;
			}

			// Make resources from current stage available for the next one.
			for (RenderGraphNode& node : stage)
			{
				for (const auto& bufferOutput : node.Node->GetBufferOutputInfos())
					availableBufferSources.insert_or_assign(bufferOutput.first, &node);

				for (const auto& imageOutput : node.Node->GetImageOutputInfos())
					availableImageSources.insert_or_assign(imageOutput.first, &node);
			}
		}

		if (!availableImageSources.contains("Output"))
		{
			Logger::Error("Render graph should contain an image entry named 'Output' to be presented at end of frame.");
			return false;
		}

		return true;
	}

	bool RenderGraph::ReserveRenderTexturesForPasses(const Renderer& renderer, const glm::uvec3& defaultExtents,
		std::unordered_map<Format, std::vector<ImageInfo>>& formatRenderTextureLookup,
		std::unordered_map<IRenderImage*, uint32_t>& imageInfoLookup)
	{
		// Reserve render textures for the passes.
		for (auto& stage : m_renderGraph)
		{
			// Reset available images.
			for (auto& imageInfos : formatRenderTextureLookup)
			{
				for (auto& info : imageInfos.second)
				{
					info.Access = AccessFlags::None;
				}
			}

			for (auto& node : stage)
			{
				// Build resources before passes.
				if (!node.Node->BuildResources(renderer))
				{
					Logger::Error("Failed to build resources for render node '{}' while building render graph.", node.Node->GetName());
					return false;
				}

				for (const auto& output : node.Node->GetBufferOutputInfos())
				{
					node.OutputBuffers[output.first] = output.second.Buffer;
				}

				if (node.Type == RenderNodeType::Resource)
				{
					for (const auto& output : node.Node->GetImageOutputInfos())
					{
						auto& imageMap = formatRenderTextureLookup[output.second.Format];
						node.OutputImages[output.first] = output.second.Image;
						imageMap.emplace_back(ImageInfo(*output.second.Image));
					}
				}
				else
				{
					const auto& inputImageInfos = node.Node->GetImageInputInfos();
					for (const auto& info : inputImageInfos)
					{
						// Attempt to match previous output, if available.
						const auto& previousOutput = node.InputImageSources.find(info.first);
						if (previousOutput != node.InputImageSources.end())
						{
							const auto image = previousOutput->second.OutputImages[info.first];
							const auto& imageInfo = imageInfoLookup.find(image);
							if (imageInfo != imageInfoLookup.end())
							{
								if ((info.second.Access & AccessFlags::Read) == AccessFlags::Read)
									formatRenderTextureLookup[image->GetFormat()][imageInfo->second].Access |= AccessFlags::Read;
								else
									formatRenderTextureLookup[image->GetFormat()][imageInfo->second].Access &= ~AccessFlags::Read;
							}
							node.InputImages[info.first] = image;
						}
						else
						{
							const RenderPassImageInfo& inputInfo = info.second;
							glm::uvec3 requestedExtents = inputInfo.Dimensions == glm::uvec3() ? defaultExtents : inputInfo.Dimensions;
							IRenderImage* image = nullptr;
							if (!TryGetOrAddImage(info.first, renderer, formatRenderTextureLookup, m_renderTextures, imageInfoLookup,
								inputInfo.Format, inputInfo.Access, requestedExtents, &image))
								return false;

							node.InputImages[info.first] = image;
						}
					}

					const auto& outputImageInfos = node.Node->GetImageOutputInfos();
					for (const auto& info : outputImageInfos)
					{
						const RenderPassImageInfo& outputInfo = info.second;
						glm::uvec3 requestedExtents = outputInfo.Dimensions == glm::uvec3() ? defaultExtents : outputInfo.Dimensions;

						IRenderImage* image = nullptr;

						const auto& matchingInput = node.InputImages.find(info.first);
						if (matchingInput != node.InputImages.end())
						{
							IRenderImage* inputImage = matchingInput->second;
							const auto& matchingInputInfo = imageInfoLookup.find(inputImage);
							if (matchingInputInfo != imageInfoLookup.end())
							{
								const auto& indices = formatRenderTextureLookup[inputImage->GetFormat()];
								bool matchingIsRead = (indices[matchingInputInfo->second].Access & AccessFlags::Read) == AccessFlags::Read;
								if (!matchingIsRead && inputImage->GetFormat() == outputInfo.Format && inputImage->GetDimensions() == requestedExtents)
								{
									image = inputImage; // Passthrough
								}
							}
						}

						if (image == nullptr && !TryGetOrAddImage(info.first, renderer, formatRenderTextureLookup, m_renderTextures, imageInfoLookup,
							outputInfo.Format, AccessFlags::Write, requestedExtents, &image))
							return false;

						node.OutputImages[info.first] = image;
					}

					const auto& inputBuffers = node.Node->GetBufferInputInfos();
					for (const auto& info : inputBuffers)
					{
						const auto& previousOutput = node.InputBufferSources.find(info.first);
						if (previousOutput != node.InputBufferSources.end())
						{
							const auto buffer = previousOutput->second.OutputBuffers[info.first];
							node.InputBuffers[info.first] = buffer;
						}
						else
						{
							Logger::Error("Could not find buffer input '{}' for render node '{}' while building render graph.", info.first, node.Node->GetName());
							return false;
						}
					}
				}
			}
		}

		return true;
	}

	bool RenderGraph::BuildPasses(const Renderer& renderer, const IDevice& device)
	{
		uint32_t concurrentFrameCount = renderer.GetConcurrentFrameCount();

		// Build the actual passes.
		uint32_t stageIndex = 0;
		m_perStageOwnershipReleaseResources.resize(m_renderGraph.size());
		for (const auto& stage : m_renderGraph)
		{
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					IRenderPass* pass = static_cast<IRenderPass*>(node.Node);
					if (!pass->Build(renderer, node.InputImages, node.OutputImages, node.InputBuffers, node.OutputBuffers))
					{
						Logger::Error("Failed to build render pass '{}' while building render graph.", pass->GetName());
						return false;
					}
				}
				else if (node.Type == RenderNodeType::Compute)
				{
					IComputePass* pass = static_cast<IComputePass*>(node.Node);
					if (!pass->Build(renderer, node.InputImages, node.OutputImages, node.InputBuffers, node.OutputBuffers))
					{
						Logger::Error("Failed to build compute pass '{}' while building render graph.", pass->GetName());
						return false;
					}
				}

				RenderPassImageInfo blankImageInfo{};
				RenderPassBufferInfo blankBufferInfo{};
				blankImageInfo.LastUsagePassType = node.Type;
				blankBufferInfo.LastUsagePassType = node.Type;
				blankImageInfo.LastUsagePassStageIndex = stageIndex;
				blankBufferInfo.LastUsagePassStageIndex = stageIndex;

				for (const auto& pair : node.Node->GetImageInputInfos())
					if (!m_imageInfoBarrierState.contains(pair.first))
						m_imageInfoBarrierState.emplace(pair.first, blankImageInfo);
					else
					{
						RenderPassImageInfo& existing = m_imageInfoBarrierState.at(pair.first);
						if (existing.LastUsagePassType == RenderNodeType::Resource)
							existing.LastUsagePassType = node.Type;
						else if (existing.LastUsagePassType != node.Type)
						{
							m_perStageOwnershipReleaseResources[existing.LastUsagePassStageIndex].push_back(std::make_pair(pair.first, node.Type == RenderNodeType::Compute));
							existing.LastUsagePassType = node.Type;
						}
						existing.LastUsagePassStageIndex = stageIndex;
					}

				for (const auto& pair : node.Node->GetImageOutputInfos())
					if (!m_imageInfoBarrierState.contains(pair.first))
						m_imageInfoBarrierState.emplace(pair.first, blankImageInfo);
					else
					{
						RenderPassImageInfo& existing = m_imageInfoBarrierState.at(pair.first);
						if (existing.LastUsagePassType == RenderNodeType::Resource)
							existing.LastUsagePassType = node.Type;
						else if (existing.LastUsagePassType != node.Type)
						{
							m_perStageOwnershipReleaseResources[existing.LastUsagePassStageIndex].push_back(std::make_pair(pair.first, node.Type == RenderNodeType::Compute));
							existing.LastUsagePassType = node.Type;
						}
						existing.LastUsagePassStageIndex = stageIndex;
					}

				for (const auto& pair : node.Node->GetBufferInputInfos())
					if (!m_bufferInfoBarrierState.contains(pair.first))
						m_bufferInfoBarrierState.emplace(pair.first, blankBufferInfo);
					else
					{
						RenderPassBufferInfo& existing = m_bufferInfoBarrierState.at(pair.first);
						if (existing.LastUsagePassType == RenderNodeType::Resource)
							existing.LastUsagePassType = node.Type;
						else if (existing.LastUsagePassType != node.Type)
						{
							m_perStageOwnershipReleaseResources[existing.LastUsagePassStageIndex].push_back(std::make_pair(pair.first, node.Type == RenderNodeType::Compute));
							existing.LastUsagePassType = node.Type;
						}
						existing.LastUsagePassStageIndex = stageIndex;
					}

				for (const auto& pair : node.Node->GetBufferOutputInfos())
					if (!m_bufferInfoBarrierState.contains(pair.first))
						m_bufferInfoBarrierState.emplace(pair.first, blankBufferInfo);
					else
					{
						RenderPassBufferInfo& existing = m_bufferInfoBarrierState.at(pair.first);
						if (existing.LastUsagePassType == RenderNodeType::Resource)
							existing.LastUsagePassType = node.Type;
						else if (existing.LastUsagePassType != node.Type)
						{
							m_perStageOwnershipReleaseResources[existing.LastUsagePassStageIndex].push_back(std::make_pair(pair.first, node.Type == RenderNodeType::Compute));
							existing.LastUsagePassType = node.Type;
						}
						existing.LastUsagePassStageIndex = stageIndex;
					}
			}

			std::vector<std::unique_ptr<ICommandBuffer>> renderCommandBuffers;
			std::vector<std::unique_ptr<ICommandBuffer>> computeCommandBuffers;
			for (uint32_t i = 0; i < concurrentFrameCount; ++i)
			{
				auto commandBuffers = m_renderCommandPools[i]->CreateCommandBuffers(std::format("Stage {} Render Command Buffer {}", stageIndex, i), device, 1);
				if (commandBuffers.empty())
					return false;
				renderCommandBuffers.emplace_back(std::move(commandBuffers[0]));

				if (m_asyncCompute)
				{
					commandBuffers = m_computeCommandPools[i]->CreateCommandBuffers(std::format("Stage {} Compute Command Buffer {}", stageIndex, i), device, 1);
					if (commandBuffers.empty())
						return false;
					computeCommandBuffers.emplace_back(std::move(commandBuffers[0]));
				}
			}

			m_renderCommandBuffers.emplace_back(std::move(renderCommandBuffers));
			if (m_asyncCompute)
				m_computeCommandBuffers.emplace_back(std::move(computeCommandBuffers));

			++stageIndex;
		}

		return true;
	}

	bool RenderGraph::FindFinalNode()
	{
		// Find the last usage of the 'Output' image and mark it as the final render pass.
		m_finalNode = nullptr;
		for (auto it = m_renderGraph.rbegin(); it != m_renderGraph.rend(); ++it)
		{
			for (const auto& node : *it)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					for (const auto& imageOutput : node.Node->GetImageOutputInfos())
					{
						if (imageOutput.first == "Output")
						{
							m_finalNode = &node;
							break;
						}
					}
				}
			}

			if (m_finalNode != nullptr)
				break;
		}

		if (m_finalNode == nullptr)
		{
			Logger::Error("Render graph could not resolve final 'Output' image usage.");
			return false;
		}

		return true;
	}

	bool RenderGraph::Build(const Renderer& renderer, bool asyncCompute)
	{
		const IDevice& device = renderer.GetDevice();
		const IPhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();

		std::unordered_map<std::string, RenderGraphNode&> renderGraphNodeLookup;
		std::unordered_map<Format, std::vector<ImageInfo>> formatRenderTextureLookup;
		std::unordered_map<IRenderImage*, uint32_t> imageInfoLookup;
		m_renderTextures.clear();
		m_renderGraph.clear();
		m_renderCommandBuffers.clear();
		m_computeCommandBuffers.clear();
		m_renderTextures.clear();
		m_imageInfoBarrierState.clear();
		m_bufferInfoBarrierState.clear();
		m_perStageOwnershipReleaseResources.clear();

		if (asyncCompute != m_asyncCompute)
		{
			if (!Initialise(physicalDevice, device, renderer.GetResourceFactory(), renderer.GetConcurrentFrameCount(), asyncCompute))
				return false;
		}

		std::unordered_map<std::string, RenderGraphNode*> availableBufferSources{};
		std::unordered_map<std::string, RenderGraphNode*> availableImageSources{};

		Format depthFormat = physicalDevice.GetDepthFormat();
		const ISwapChain& swapchain = renderer.GetSwapChain();
		glm::uvec3 defaultExtents = glm::uvec3(swapchain.GetExtent(), 1);
		Format swapchainFormat = swapchain.GetFormat();

		auto nodeLookupKeys = std::views::keys(m_renderNodeLookup);
		std::vector<std::string_view> passNames{ nodeLookupKeys.begin(), nodeLookupKeys.end() };

		std::vector<IRenderNode*> renderNodeStack;
		for (auto& renderResource : m_renderResources)
		{
			if (renderResource->GetEnabled())
			{
				renderResource->UpdateConnections(renderer, passNames);
				renderNodeStack.push_back(renderResource);
			}
			else
			{
				renderResource->ClearResources();
			}
		}

		uint32_t enabledPasses = 0;
		renderNodeStack.reserve(m_renderPasses.size() + m_computePasses.size());
		for (const auto& pass : m_computePasses)
		{
			if (pass->GetEnabled())
			{
				pass->UpdateConnections(renderer, passNames);
				pass->UpdatePlaceholderFormats(swapchainFormat, depthFormat);
				renderNodeStack.emplace_back(pass);
				++enabledPasses;
			}
			else
			{
				pass->ClearResources();
			}
		}
		for (const auto& pass : m_renderPasses)
		{
			if (pass->GetEnabled())
			{
				pass->UpdateConnections(renderer, passNames);
				pass->UpdatePlaceholderFormats(swapchainFormat, depthFormat);

				renderNodeStack.emplace_back(pass);
				++enabledPasses;
			}
			else
			{
				pass->ClearResources();
			}
		}

		if (!DetermineRequiredResources(renderNodeStack, availableImageSources, availableBufferSources))
			return false;

		if (!ReserveRenderTexturesForPasses(renderer, defaultExtents, formatRenderTextureLookup, imageInfoLookup))
			return false;

		if (!BuildPasses(renderer, device))
			return false;

		if (!FindFinalNode())
			return false;

		if (!m_renderStats.Initialise(renderer.GetPhysicalDevice(), device, enabledPasses))
			return false;

		m_dirty = false;
		return true;
	}

	void RenderGraph::TransitionResourcesForStage(const Renderer& renderer, const ICommandBuffer& commandBuffer, bool isCompute,
		const std::vector<RenderGraphNode>& nodes)
	{
		const IDevice& device = renderer.GetDevice();
		uint32_t currentQueueFamilyIndex = commandBuffer.GetQueueFamilyIndex();

		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(renderer.GetResourceFactory().CreateMemoryBarriers());

		// Transition image layouts where necessary.
		for (const auto& node : nodes)
		{
			if (node.Type == RenderNodeType::Resource)
				continue;

			if (!m_asyncCompute || (node.Type == RenderNodeType::Compute && isCompute) || (node.Type == RenderNodeType::Pass && !isCompute))
			{
				for (const auto& pair : node.Node->GetImageInputInfos())
				{
					const RenderPassImageInfo& imageInfo = pair.second;
					if (imageInfo.Image != nullptr && imageInfo.Access != AccessFlags::None)
					{
						auto& currentState = m_imageInfoBarrierState.at(pair.first);
						currentState.Image = imageInfo.Image;

						if (currentState.QueueFamilyIndex == ~0U)
							currentState.QueueFamilyIndex = currentQueueFamilyIndex;

						// If image layout is currently undefined, clear it as it may be being used as a read-only input.
						if (imageInfo.Image->GetLayout() == ImageLayout::Undefined && imageInfo.Layout == ImageLayout::ShaderReadOnly)
						{
							imageInfo.Image->AppendImageLayoutTransitionExt(commandBuffer,
								MaterialStageFlags::Transfer, ImageLayout::TransferDst, MaterialAccessFlags::TransferWrite, *memoryBarriers);
							commandBuffer.MemoryBarrier(*memoryBarriers);
							memoryBarriers->Clear();

							if (imageInfo.Format == renderer.GetDepthFormat())
								commandBuffer.ClearDepthStencilImage(*imageInfo.Image);
							else
								commandBuffer.ClearColourImage(*imageInfo.Image, Engine::Colour());
						}

						imageInfo.Image->AppendImageLayoutTransitionExt(commandBuffer, imageInfo.StageFlags, imageInfo.Layout, imageInfo.MatAccessFlags,
							*memoryBarriers, 0, 0, currentState.QueueFamilyIndex, currentQueueFamilyIndex, isCompute);

						currentState.StageFlags = imageInfo.StageFlags;
						currentState.MatAccessFlags = imageInfo.MatAccessFlags;
						currentState.Layout = imageInfo.Layout;
						currentState.QueueFamilyIndex = currentQueueFamilyIndex;
					}
				}

				for (const auto& pair : node.Node->GetImageOutputInfos())
				{
					const RenderPassImageInfo& imageInfo = pair.second;
					if (imageInfo.Image != nullptr && imageInfo.Access != AccessFlags::None)
					{
						auto& currentState = m_imageInfoBarrierState.at(pair.first);
						currentState.Image = imageInfo.Image;

						if (currentState.QueueFamilyIndex == ~0U)
							currentState.QueueFamilyIndex = currentQueueFamilyIndex;

						imageInfo.Image->AppendImageLayoutTransitionExt(commandBuffer, imageInfo.StageFlags, imageInfo.Layout, imageInfo.MatAccessFlags,
							*memoryBarriers, 0, 0, currentState.QueueFamilyIndex, currentQueueFamilyIndex, isCompute);

						currentState.StageFlags = imageInfo.StageFlags;
						currentState.MatAccessFlags = imageInfo.MatAccessFlags;
						currentState.Layout = imageInfo.Layout;
						currentState.QueueFamilyIndex = currentQueueFamilyIndex;
					}
				}
			}
		}

		// Perform buffer memory barriers where necessary.

		std::unordered_map<std::string, RenderPassBufferInfo> bufferInfos{};
		for (const auto& node : nodes)
		{
			if (!m_asyncCompute || (node.Type == RenderNodeType::Pass && !isCompute) || (node.Type == RenderNodeType::Compute && isCompute))
			{
				const auto& bufferInputInfos = node.Node->GetBufferInputInfos();
				const auto& bufferOutputInfos = node.Node->GetBufferOutputInfos();
				bufferInfos.insert(bufferInputInfos.begin(), bufferInputInfos.end());
				bufferInfos.insert(bufferOutputInfos.begin(), bufferOutputInfos.end());
			}
		}

		if (!bufferInfos.empty())
		{
			for (const auto& pair : bufferInfos)
			{
				const RenderPassBufferInfo& bufferInfo = pair.second;
				if (bufferInfo.StageFlags != MaterialStageFlags::None && bufferInfo.Buffer != nullptr)
				{
					auto& currentState = m_bufferInfoBarrierState.at(pair.first);
					currentState.Buffer = bufferInfo.Buffer;

					if (currentState.QueueFamilyIndex == ~0U)
						currentState.QueueFamilyIndex = currentQueueFamilyIndex;

					bufferInfo.Buffer->AppendBufferMemoryBarrier(commandBuffer, currentState.StageFlags, currentState.MatAccessFlags,
						bufferInfo.StageFlags, bufferInfo.MatAccessFlags, *memoryBarriers,
						currentState.QueueFamilyIndex, currentQueueFamilyIndex);

					currentState.Access = bufferInfo.Access;
					currentState.StageFlags = bufferInfo.StageFlags;
					currentState.MatAccessFlags = bufferInfo.MatAccessFlags;
					currentState.QueueFamilyIndex = currentQueueFamilyIndex;
				}
			}
		}

		if (!memoryBarriers->Empty())
			commandBuffer.MemoryBarrier(*memoryBarriers);
	}

	bool RenderGraph::DrawRenderPass(const Renderer& renderer, const RenderGraphNode& node, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex, const glm::uvec2& size) const
	{
		const IDevice& device = renderer.GetDevice();

		IRenderPass* pass = static_cast<IRenderPass*>(node.Node);

		m_renderStats.Begin(commandBuffer, pass->GetName(), false);

		const std::vector<AttachmentInfo>& colourAttachments = pass->GetColourAttachments();
		const std::optional<AttachmentInfo>& depthAttachment = pass->GetDepthAttachment();

		glm::uvec2 passSize = size;
		pass->GetCustomSize(passSize);

		pass->PreDraw(renderer, commandBuffer, passSize, frameIndex, node.InputImages, node.OutputImages);

		uint32_t layerCount = pass->GetLayerCount();
		for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
		{
			commandBuffer.BeginRendering(colourAttachments, depthAttachment, passSize, layerCount);

			pass->Draw(renderer, commandBuffer, passSize, frameIndex, layerIndex);

			commandBuffer.EndRendering();
		}

		pass->PostDraw(renderer, commandBuffer, passSize, frameIndex, node.InputImages, node.OutputImages);

		m_renderStats.End(commandBuffer, false);

		return true;
	}

	bool RenderGraph::DispatchComputePass(Renderer& renderer, const RenderGraphNode& node, const ICommandBuffer& commandBuffer,
		uint32_t frameIndex) const
	{
		IComputePass* pass = static_cast<IComputePass*>(node.Node);

		m_renderStats.Begin(commandBuffer, pass->GetName(), true);

		pass->Dispatch(renderer, commandBuffer, frameIndex);

		m_renderStats.End(commandBuffer, true);

		return true;
	}

	bool RenderGraph::BlitToSwapchain(Renderer& renderer, const IDevice& device, uint32_t frameIndex,
		std::vector<SubmitInfo>& renderSubmitInfos, std::vector<SubmitInfo>& computeSubmitInfos) const
	{
		ICommandBuffer& blitCommandBuffer = *m_blitCommandBuffers[frameIndex];
		if (!blitCommandBuffer.Begin())
		{
			return false;
		}

		IRenderImage* finalImage = m_finalNode->OutputImages.at("Output");

		const glm::uvec3& extents = finalImage->GetDimensions();
		const glm::uvec3 offset(extents.x, extents.y, extents.z);

		ImageBlit blit{};
		blit.srcSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.srcOffsets = std::array<glm::uvec3, 2>{ glm::uvec3(), offset };
		blit.dstSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.dstOffsets = std::array<glm::uvec3, 2>{ glm::uvec3(), offset };

		std::unique_ptr<IMemoryBarriers> memoryBarriers = std::move(renderer.GetResourceFactory().CreateMemoryBarriers());

		IRenderImage& presentImage = renderer.GetPresentImage();
		finalImage->AppendImageLayoutTransition(blitCommandBuffer, ImageLayout::TransferSrc, *memoryBarriers);
		presentImage.AppendImageLayoutTransition(blitCommandBuffer, ImageLayout::TransferDst, *memoryBarriers);
		blitCommandBuffer.MemoryBarrier(*memoryBarriers);
		memoryBarriers->Clear();

		blitCommandBuffer.BlitImage(*finalImage, presentImage, { blit }, Filter::Linear);
		presentImage.AppendImageLayoutTransition(blitCommandBuffer, ImageLayout::PresentSrc, *memoryBarriers);
		blitCommandBuffer.MemoryBarrier(*memoryBarriers);
		blitCommandBuffer.End();

		SubmitInfo& blitSubmitInfo = renderSubmitInfos.emplace_back();
		blitSubmitInfo.CommandBuffers.emplace_back(&blitCommandBuffer);

		m_renderStats.FinaliseResults(renderer.GetPhysicalDevice(), device, m_renderResources);

		return renderer.Present(renderSubmitInfos, computeSubmitInfos);
	}

	void RenderGraph::ReleaseResourceQueueFamilyOwnership(Renderer& renderer, const ICommandBuffer& renderCommandBuffer,
		const ICommandBuffer& computeCommandBuffer, const std::vector<std::pair<std::string, bool>>& resourcesToTransfer,
		bool& renderToCompute, bool& computeToRender)
	{
		renderToCompute = false;
		computeToRender = false;

		if (resourcesToTransfer.empty())
			return;

		const IResourceFactory& resourceFactory = renderer.GetResourceFactory();
		std::unique_ptr<IMemoryBarriers> renderToComputeMemoryBarriers = std::move(resourceFactory.CreateMemoryBarriers());
		std::unique_ptr<IMemoryBarriers> computeToRenderMemoryBarriers = std::move(resourceFactory.CreateMemoryBarriers());

		uint32_t renderQueueFamilyIndex = renderCommandBuffer.GetQueueFamilyIndex();
		uint32_t computeQueueFamilyIndex = computeCommandBuffer.GetQueueFamilyIndex();

		for (const auto& pair : resourcesToTransfer)
		{
			const std::string& resourceName = pair.first;
			bool renderToCompute = pair.second;

			if (renderToCompute)
			{
				auto bufferIterator = m_bufferInfoBarrierState.find(resourceName);
				if (bufferIterator != m_bufferInfoBarrierState.end())
				{
					auto& bufferResource = bufferIterator->second;
					if (bufferResource.Buffer != nullptr)
					{
						bufferResource.Buffer->AppendBufferMemoryBarrier(renderCommandBuffer, bufferResource.StageFlags, bufferResource.MatAccessFlags,
							MaterialStageFlags::BottomOfPipe, MaterialAccessFlags::None,
							*renderToComputeMemoryBarriers, renderQueueFamilyIndex, computeQueueFamilyIndex);
					}
				}
				else
				{
					auto& imageResource = m_imageInfoBarrierState.at(resourceName);
					if (imageResource.Image != nullptr)
					{
						imageResource.Image->AppendImageLayoutTransition(renderCommandBuffer, ImageLayout::ShaderReadOnly,
							*renderToComputeMemoryBarriers, renderQueueFamilyIndex, computeQueueFamilyIndex, false);
					}
				}
			}
			else
			{
				const auto& bufferIterator = m_bufferInfoBarrierState.find(resourceName);
				if (bufferIterator != m_bufferInfoBarrierState.end())
				{
					auto& bufferResource = bufferIterator->second;
					if (bufferResource.Buffer != nullptr)
					{
						bufferResource.Buffer->AppendBufferMemoryBarrier(computeCommandBuffer, bufferResource.StageFlags, bufferResource.MatAccessFlags,
							MaterialStageFlags::BottomOfPipe, MaterialAccessFlags::None,
							*computeToRenderMemoryBarriers, computeQueueFamilyIndex, renderQueueFamilyIndex);
					}
				}
				else
				{
					const auto& imageResource = m_imageInfoBarrierState.at(resourceName);
					if (imageResource.Image != nullptr)
						imageResource.Image->AppendImageLayoutTransition(computeCommandBuffer, ImageLayout::ShaderReadOnly,
							*computeToRenderMemoryBarriers, computeQueueFamilyIndex, renderQueueFamilyIndex, true);
				}
			}
		}

		if (!renderToComputeMemoryBarriers->Empty())
		{
			renderCommandBuffer.MemoryBarrier(*renderToComputeMemoryBarriers);
			renderToCompute = true;
		}

		if (!computeToRenderMemoryBarriers->Empty())
		{
			computeCommandBuffer.MemoryBarrier(*computeToRenderMemoryBarriers);
			computeToRender = true;
		}
	}

	bool RenderGraph::Draw(Renderer& renderer, uint32_t frameIndex)
	{
		const IDevice& device = renderer.GetDevice();
		const glm::uvec2& size = renderer.GetSwapChain().GetExtent();
		std::vector<SubmitInfo> renderSubmitInfos;
		std::vector<SubmitInfo> computeSubmitInfos;

		m_renderCommandPools[frameIndex]->Reset(device);
		if (m_asyncCompute)
			m_computeCommandPools[frameIndex]->Reset(device);

		// Reset family queue index for resource state tracking at start of frame to indicate their first usage do not require a queue family ownership transfer.
		for (auto& bufferState : m_bufferInfoBarrierState)
			bufferState.second.QueueFamilyIndex = ~0U;
		for (auto& imageState : m_imageInfoBarrierState)
			imageState.second.QueueFamilyIndex = ~0U;

		uint32_t stageIndex = 0;

		bool lastStageUsedRenderToComputeSemaphore = false;
		bool lastStageUsedComputeToRenderSemaphore = false;
		for (const auto& stage : m_renderGraph)
		{
			const ICommandBuffer& renderCommandBuffer = *m_renderCommandBuffers[stageIndex][frameIndex];
			if (!renderCommandBuffer.Begin())
			{
				return false;
			}

			const ICommandBuffer* computeCommandBuffer = &renderCommandBuffer;
			if (m_asyncCompute)
			{
				computeCommandBuffer = m_computeCommandBuffers[stageIndex][frameIndex].get();
				if (!computeCommandBuffer->Begin())
				{
					return false;
				}
			}

			SubmitInfo renderSubmitInfo{};
			SubmitInfo computeSubmitInfo{};

			// Perform resource transitions in bulk, per stage.

			bool stageHasRenderPasses = false;
			TransitionResourcesForStage(renderer, renderCommandBuffer, false, stage);
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					if (!DrawRenderPass(renderer, node, renderCommandBuffer, frameIndex, size))
						return false;
					stageHasRenderPasses = true;
				}
			}

			bool stageHasComputePasses = false;
			if (m_asyncCompute)
				TransitionResourcesForStage(renderer, *computeCommandBuffer, true, stage);
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Compute)
				{
					if (!DispatchComputePass(renderer, node, *computeCommandBuffer, frameIndex))
						return false;
					stageHasComputePasses = true;
				}
			}

			bool renderToComputeSemaphore = false;
			bool computeToRenderSemaphore = false;
			if (m_asyncCompute)
			{
				ReleaseResourceQueueFamilyOwnership(renderer, renderCommandBuffer, *computeCommandBuffer,
					m_perStageOwnershipReleaseResources[stageIndex], renderToComputeSemaphore, computeToRenderSemaphore);
			}

			renderCommandBuffer.End();
			if (m_asyncCompute)
				computeCommandBuffer->End();

			if (lastStageUsedComputeToRenderSemaphore && stageHasRenderPasses)
			{
				renderSubmitInfo.WaitSemaphores.emplace_back(m_computeToRenderSemaphore.get());
				renderSubmitInfo.WaitValues.emplace_back(m_computeToRenderSemaphore->Value);
				renderSubmitInfo.Stages.emplace_back(MaterialStageFlags::TopOfPipe);
				lastStageUsedComputeToRenderSemaphore = false;
			}

			if (lastStageUsedRenderToComputeSemaphore && m_asyncCompute && stageHasComputePasses)
			{
				computeSubmitInfo.WaitSemaphores.emplace_back(m_renderToComputeSemaphore.get());
				computeSubmitInfo.WaitValues.emplace_back(m_renderToComputeSemaphore->Value);
				computeSubmitInfo.Stages.emplace_back(MaterialStageFlags::TopOfPipe);
				lastStageUsedRenderToComputeSemaphore = false;
			}

			if (stageHasRenderPasses || (!m_asyncCompute && stageHasComputePasses))
			{
				renderSubmitInfo.CommandBuffers.emplace_back(&renderCommandBuffer);
				if (renderToComputeSemaphore)
				{
					++m_renderToComputeSemaphore->Value;
					renderSubmitInfo.SignalSemaphores.emplace_back(m_renderToComputeSemaphore.get());
					renderSubmitInfo.SignalValues.emplace_back(m_renderToComputeSemaphore->Value);
					lastStageUsedRenderToComputeSemaphore = true;
				}
				renderSubmitInfos.emplace_back(std::move(renderSubmitInfo));
			}

			if (m_asyncCompute && stageHasComputePasses)
			{
				computeSubmitInfo.CommandBuffers.emplace_back(computeCommandBuffer);
				if (computeToRenderSemaphore)
				{
					++m_computeToRenderSemaphore->Value;
					computeSubmitInfo.SignalSemaphores.emplace_back(m_computeToRenderSemaphore.get());
					computeSubmitInfo.SignalValues.emplace_back(m_computeToRenderSemaphore->Value);
					lastStageUsedComputeToRenderSemaphore = true;
				}
				computeSubmitInfos.emplace_back(std::move(computeSubmitInfo));
			}

			++stageIndex;
		}

		return BlitToSwapchain(renderer, device, frameIndex, renderSubmitInfos, computeSubmitInfos);
	}
}