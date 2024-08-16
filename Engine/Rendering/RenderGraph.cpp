#include "RenderGraph.hpp"
#include "Renderer.hpp"
#include "Core/Logger.hpp"
#include "IMaterialManager.hpp"
#include "QueueFamilyIndices.hpp"
#include "Resources/ICommandBuffer.hpp"
#include "Resources/ICommandPool.hpp"
#include "Resources/ISemaphore.hpp"
#include "Resources/IImageMemoryBarriers.hpp"
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
		, m_finalNode(nullptr)
		, m_renderCommandBuffers()
		, m_computeCommandBuffers()
		, m_renderCommandPools()
		, m_computeCommandPools()
		, m_renderSemaphore(nullptr)
		, m_computeSemaphore(nullptr)
		, m_renderTextures()
		, m_renderStats(renderStats)
		, m_blitCommandBuffers()
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

	bool RenderGraph::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, const IResourceFactory& resourceFactory, uint32_t concurrentFrameCount)
	{
		const QueueFamilyIndices& indices = physicalDevice.GetQueueFamilyIndices();

		m_renderSemaphore = std::move(resourceFactory.CreateGraphicsSemaphore());
		if (!m_renderSemaphore->Initialise("GraphicsSemaphore", device))
		{
			return false;
		}

		m_computeSemaphore = std::move(resourceFactory.CreateGraphicsSemaphore());
		if (!m_computeSemaphore->Initialise("ComputeSemaphore", device))
		{
			return false;
		}

		for (uint32_t i = 0; i < concurrentFrameCount; ++i)
		{
			m_renderCommandPools.emplace_back(std::move(resourceFactory.CreateCommandPool()));
			if (!m_renderCommandPools[i]->Initialise("RenderCommandPool", physicalDevice, device, indices.GraphicsFamily.value(), CommandPoolFlags::Reset))
			{
				return false;
			}

			auto commandBuffers = m_renderCommandPools[i]->CreateCommandBuffers("BlitCommandBuffer", device, 1);
			if (commandBuffers.empty())
			{
				return false;
			}
			m_blitCommandBuffers.emplace_back(std::move(commandBuffers[0]));

			// TODO: Sort out render-compute synchronization.
			m_computeCommandPools.emplace_back(std::move(resourceFactory.CreateCommandPool()));
			//if (!m_computeCommandPools[i]->Initialise("ComputeCommandPool", physicalDevice, device, indices.ComputeFamily.value(), CommandPoolFlags::Reset))
			if (!m_computeCommandPools[i]->Initialise("ComputeCommandPool", physicalDevice, device, indices.GraphicsFamily.value(), CommandPoolFlags::Reset))
			{
				return false;
			}
		}

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

		if (pass->second->GetNodeType() == RenderNodeType::Pass)
		{
			static_cast<IRenderPass*>(pass->second)->SetEnabled(enabled);
		}
		else if (pass->second->GetNodeType() == RenderNodeType::Compute)
		{
			static_cast<IComputePass*>(pass->second)->SetEnabled(enabled);
		}

		m_dirty = true;
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
			Logger::Error("Placeholder format should be handled by IRenderPass::UpdatePlaceholderFormats.");
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
			usageFlags = ImageUsageFlags::DepthStencilAttachment | ImageUsageFlags::Sampled;
		}
		else
		{
			aspectFlags = ImageAspectFlags::Color;
			usageFlags = ImageUsageFlags::ColorAttachment | ImageUsageFlags::Sampled | ImageUsageFlags::TransferSrc;
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
			}

			std::vector<std::unique_ptr<ICommandBuffer>> renderCommandBuffers;
			std::vector<std::unique_ptr<ICommandBuffer>> computeCommandBuffers;
			for (uint32_t i = 0; i < concurrentFrameCount; ++i)
			{
				auto commandBuffers = m_renderCommandPools[i]->CreateCommandBuffers(std::format("Stage {} Render Command Buffer {}", stageIndex, i), device, 1);
				if (commandBuffers.empty())
					return false;
				renderCommandBuffers.emplace_back(std::move(commandBuffers[0]));

				commandBuffers = m_computeCommandPools[i]->CreateCommandBuffers(std::format("Stage {} Compute Command Buffer {}", stageIndex, i), device, 1);
				if (commandBuffers.empty())
					return false;
				computeCommandBuffers.emplace_back(std::move(commandBuffers[0]));
			}

			m_renderCommandBuffers.emplace_back(std::move(renderCommandBuffers));
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

	bool RenderGraph::Build(const Renderer& renderer)
	{
		const IDevice& device = renderer.GetDevice();

		std::unordered_map<std::string, RenderGraphNode&> renderGraphNodeLookup;
		std::unordered_map<Format, std::vector<ImageInfo>> formatRenderTextureLookup;
		std::unordered_map<IRenderImage*, uint32_t> imageInfoLookup;
		m_renderTextures.clear();
		m_renderGraph.clear();
		m_renderCommandBuffers.clear();
		m_computeCommandBuffers.clear();
		m_renderTextures.clear();

		std::unordered_map<std::string, RenderGraphNode*> availableBufferSources{};
		std::unordered_map<std::string, RenderGraphNode*> availableImageSources{};

		const IPhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();
		Format depthFormat = physicalDevice.GetDepthFormat();
		const ISwapChain& swapchain = renderer.GetSwapChain();
		glm::uvec3 defaultExtents = glm::uvec3(swapchain.GetExtent(), 1);
		Format swapchainFormat = swapchain.GetFormat();

		auto nodeLookupKeys = std::views::keys(m_renderNodeLookup);
		std::vector<std::string_view> passNames{ nodeLookupKeys.begin(), nodeLookupKeys.end() };

		std::vector<IRenderNode*> renderNodeStack;
		for (auto& renderResource : m_renderResources)
		{
			renderResource->UpdateConnections(renderer, passNames);
			renderNodeStack.push_back(renderResource);
		}

		uint32_t enabledPasses = 0;
		renderNodeStack.reserve(m_renderPasses.size() + m_computePasses.size());
		for (const auto& pass : m_computePasses)
		{
			if (pass->GetEnabled())
			{
				pass->UpdateConnections(renderer, passNames);
				renderNodeStack.emplace_back(pass);
				++enabledPasses;
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

	void RenderGraph::TransitionImageLayoutsForStage(const Renderer& renderer, const ICommandBuffer& commandBuffer, const std::vector<RenderGraphNode>& nodes) const
	{
		const IDevice& device = renderer.GetDevice();

		std::unique_ptr<IImageMemoryBarriers> imageMemoryBarriers = std::move(renderer.GetResourceFactory().CreateImageMemoryBarriers());
		for (const auto& node : nodes)
		{
			if (node.Type != RenderNodeType::Pass)
				continue;

			IRenderPass* pass = static_cast<IRenderPass*>(node.Node);

			for (const auto& imageInput : node.InputImages)
			{
				imageInput.second->AppendImageLayoutTransition(device, commandBuffer, ImageLayout::ShaderReadOnly, *imageMemoryBarriers);
			}

			const std::vector<AttachmentInfo>& colourAttachments = pass->GetColourAttachments();
			for (const auto& colourAttachment : colourAttachments)
			{
				colourAttachment.renderImage->AppendImageLayoutTransition(device, commandBuffer, ImageLayout::ColorAttachment, *imageMemoryBarriers);
			}

			const std::optional<AttachmentInfo>& depthAttachment = pass->GetDepthAttachment();

			if (depthAttachment.has_value())
			{
				depthAttachment->renderImage->AppendImageLayoutTransition(device, commandBuffer, ImageLayout::DepthStencilAttachment, *imageMemoryBarriers);
			}
		}

		if (!imageMemoryBarriers->Empty())
			commandBuffer.TransitionImageLayouts(*imageMemoryBarriers);
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
		blitCommandBuffer.Reset();
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

		std::unique_ptr<IImageMemoryBarriers> imageMemoryBarriers = std::move(renderer.GetResourceFactory().CreateImageMemoryBarriers());

		IRenderImage& presentImage = renderer.GetPresentImage();
		finalImage->AppendImageLayoutTransition(device, blitCommandBuffer, ImageLayout::TransferSrc, *imageMemoryBarriers);
		presentImage.AppendImageLayoutTransition(device, blitCommandBuffer, ImageLayout::TransferDst, *imageMemoryBarriers);
		blitCommandBuffer.TransitionImageLayouts(*imageMemoryBarriers);
		imageMemoryBarriers->Clear();

		blitCommandBuffer.BlitImage(*finalImage, presentImage, { blit }, Filter::Linear);
		presentImage.AppendImageLayoutTransition(device, blitCommandBuffer, ImageLayout::PresentSrc, *imageMemoryBarriers);
		blitCommandBuffer.TransitionImageLayouts(*imageMemoryBarriers);
		blitCommandBuffer.End();

		++m_renderSemaphore->Value;
		SubmitInfo& blitSubmitInfo = renderSubmitInfos.emplace_back();
		blitSubmitInfo.CommandBuffers.emplace_back(&blitCommandBuffer);
		blitSubmitInfo.WaitSemaphores.emplace_back(m_renderSemaphore.get());
		blitSubmitInfo.WaitValues.emplace_back(m_renderSemaphore->Value - 1);
		blitSubmitInfo.Stages.emplace_back(MaterialStageFlags::Transfer);
		blitSubmitInfo.SignalSemaphores.emplace_back(m_renderSemaphore.get());
		blitSubmitInfo.SignalValues.emplace_back(m_renderSemaphore->Value);

		m_renderStats.FinaliseResults(renderer.GetPhysicalDevice(), device, m_renderResources);

		return renderer.Present(renderSubmitInfos, computeSubmitInfos);
	}

	bool RenderGraph::Draw(Renderer& renderer, uint32_t frameIndex) const
	{
		const IDevice& device = renderer.GetDevice();
		const glm::uvec2& size = renderer.GetSwapChain().GetExtent();
		std::vector<SubmitInfo> renderSubmitInfos;
		std::vector<SubmitInfo> computeSubmitInfos;

		m_renderCommandPools[frameIndex]->Reset(device);
		m_computeCommandPools[frameIndex]->Reset(device);

		bool firstStageWithRenderPassesInGraph = true;
		bool firstStageWithComputePassesInGraph = true;
		uint32_t stageIndex = 0;
		for (const auto& stage : m_renderGraph)
		{
			const ICommandBuffer& renderCommandBuffer = *m_renderCommandBuffers[stageIndex][frameIndex];
			const ICommandBuffer& computeCommandBuffer = *m_computeCommandBuffers[stageIndex][frameIndex];

			if (!renderCommandBuffer.Begin() || !computeCommandBuffer.Begin())
			{
				return false;
			}

			SubmitInfo renderSubmitInfo;
			SubmitInfo computeSubmitInfo;

			// Perform image layout transitions in bulk, per stage.
			TransitionImageLayoutsForStage(renderer, renderCommandBuffer, stage);

			bool stageHasRenderPasses = false;
			bool stageHasComputePasses = false;
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					if (!DrawRenderPass(renderer, node, renderCommandBuffer, frameIndex, size))
						return false;
					stageHasRenderPasses = true;
				}
				else if (node.Type == RenderNodeType::Compute)
				{
					if (!DispatchComputePass(renderer, node, computeCommandBuffer, frameIndex))
						return false;
					stageHasComputePasses = true;
				}
			}

			renderCommandBuffer.End();
			computeCommandBuffer.End();

			if (stageHasRenderPasses)
			{
				renderSubmitInfo.CommandBuffers.emplace_back(&renderCommandBuffer);

				if (firstStageWithRenderPassesInGraph)
				{
					++m_renderSemaphore->Value;

					renderSubmitInfo.WaitSemaphores.emplace_back(m_renderSemaphore.get());
					renderSubmitInfo.WaitValues.emplace_back(m_renderSemaphore->Value - 1);
					renderSubmitInfo.Stages.emplace_back(MaterialStageFlags::TopOfPipe);

					renderSubmitInfo.SignalSemaphores.emplace_back(m_renderSemaphore.get());
					renderSubmitInfo.SignalValues.emplace_back(m_renderSemaphore->Value);
					firstStageWithRenderPassesInGraph = false;
				}

				renderSubmitInfos.emplace_back(std::move(renderSubmitInfo));
			}

			if (stageHasComputePasses)
			{
				computeSubmitInfo.CommandBuffers.emplace_back(&computeCommandBuffer);

				if (firstStageWithComputePassesInGraph)
				{
					++m_computeSemaphore->Value;

					computeSubmitInfo.WaitSemaphores.emplace_back(m_computeSemaphore.get());
					computeSubmitInfo.WaitValues.emplace_back(m_computeSemaphore->Value - 1);
					computeSubmitInfo.Stages.emplace_back(MaterialStageFlags::TopOfPipe);

					computeSubmitInfo.SignalSemaphores.emplace_back(m_computeSemaphore.get());
					computeSubmitInfo.SignalValues.emplace_back(m_computeSemaphore->Value);
					firstStageWithComputePassesInGraph = false;
				}

				// TODO: Sort out render-compute synchronization.
				//computeSubmitInfos.emplace_back(std::move(computeSubmitInfo));
				renderSubmitInfos.emplace_back(std::move(computeSubmitInfo));
			}

			++stageIndex;
		}

		return BlitToSwapchain(renderer, device, frameIndex, renderSubmitInfos, computeSubmitInfos);
	}
}