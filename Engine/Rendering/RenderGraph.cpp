#include "RenderGraph.hpp"
#include "Renderer.hpp"
#include "Core/Logging/Logger.hpp"
#include "IMaterialManager.hpp"
#include "QueueFamilyIndices.hpp"
#include "Resources/ICommandBuffer.hpp"
#include "Resources/ICommandPool.hpp"
#include "Resources/ISemaphore.hpp"
#include "IResourceFactory.hpp"
#include "IPhysicalDevice.hpp"
#include <unordered_set>
#include <ranges>

using namespace Engine::Logging;

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
		, m_renderCommandPool(nullptr)
		, m_computeCommandPool(nullptr)
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

		m_renderCommandPool = std::move(resourceFactory.CreateCommandPool());
		if (!m_renderCommandPool->Initialise("RenderCommandPool", physicalDevice, device, indices.GraphicsFamily.value(), CommandPoolFlags::Reset))
		{
			return false;
		}

		m_blitCommandBuffers = std::move(m_renderCommandPool->CreateCommandBuffers("BlitCommandBuffer", device, concurrentFrameCount));
		if (m_blitCommandBuffers.empty())
		{
			return false;
		}

		// TODO: Sort out render-compute synchronization.
		m_computeCommandPool = std::move(resourceFactory.CreateCommandPool());
		//if (!m_computeCommandPool->Initialise(physicalDevice, device, indices.ComputeFamily.value(), CommandPoolFlags::Reset))
		if (!m_computeCommandPool->Initialise("ComputeCommandPool", physicalDevice, device, indices.GraphicsFamily.value(), CommandPoolFlags::Reset))
		{
			return false;
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

		std::unordered_map<std::string, IBuffer*> inputBuffers;
		std::unordered_map<std::string, IBuffer*> outputBuffers;

		if (renderNode->GetNodeType() == RenderNodeType::Pass)
		{
			IRenderPass* renderPass = static_cast<IRenderPass*>(renderNode);
			inputBuffers = renderPass->GetBufferInputs();
			outputBuffers = renderPass->GetBufferOutputs();
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
			inputBuffers = computePass->GetBufferInputs();
			outputBuffers = computePass->GetBufferOutputs();

			if (!computePass->Initialise(materialManager))
				return false;

			m_computePasses.emplace_back(computePass);
		}
		else
		{
			Logger::Error("Render graph only supports render passes and compute passes.");
			return false;
		}

		for (const auto& bufferOutput : outputBuffers)
		{
			if (m_bufferResourceNodeLookup.contains(bufferOutput.first))
			{
				if (inputBuffers.contains(bufferOutput.first))
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

		for (const auto bufferOutput : renderResource->GetBufferOutputs())
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

	inline bool RenderGraph::TryGetOrAddImage(const Renderer& renderer, std::unordered_map<Format, std::vector<ImageInfo>>& formatRenderTextureLookup,
		std::vector<std::unique_ptr<IRenderImage>>& renderTextures, std::unordered_map<IRenderImage*, uint32_t>& imageInfoLookup,
		Format format, bool read, bool write, const glm::uvec3& dimensions, IRenderImage** result) const
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
			if (read && !info.Write)
			{
				info.Read = true;
				*result = &info.Image;
				return true;
			}

			// Allow write if no existing reads or writes exist.
			if (write && !info.Read && !info.Write)
			{
				info.Write = true;
				*result = &info.Image;
				return true;
			}

			// Passthrough (e.g. directly received from a render resource.)
			if (!read && !write)
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

		if (!image->Initialise("RenderGraphImage", device, ImageType::e2D, format, dimensions, 1, 1, ImageTiling::Optimal,
			usageFlags, aspectFlags, MemoryUsage::AutoPreferDevice, AllocationCreateFlags::None, SharingMode::Exclusive))
		{
			*result = nullptr;
			return false;
		}

		imageInfoLookup.emplace(image.get(), static_cast<uint32_t>(availableImages.size()));
		ImageInfo& newInfo = availableImages.emplace_back(ImageInfo(*image));
		newInfo.Read = read;
		newInfo.Write = write;

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

				for (const auto& bufferInput : node.Node->GetBufferInputs())
				{
					const auto& search = stageAvailableBufferSources.find(bufferInput.first);
					if (search != stageAvailableBufferSources.end())
					{
						// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
						node.InputBufferSources.emplace(bufferInput.first, *search->second);
						for (const auto& bufferOutput : node.Node->GetBufferOutputs())
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
				for (const auto& bufferOutput : node.Node->GetBufferOutputs())
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
					info.Read = false;
					info.Write = false;
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

				for (const auto& output : node.Node->GetBufferOutputs())
				{
					node.OutputBuffers[output.first] = output.second;
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
								formatRenderTextureLookup[image->GetFormat()][imageInfo->second].Read = info.second.IsRead;
							}
							node.InputImages[info.first] = image;
						}
						else
						{
							const RenderPassImageInfo& inputInfo = info.second;
							glm::uvec3 requestedExtents = inputInfo.Dimensions == glm::uvec3() ? defaultExtents : inputInfo.Dimensions;
							IRenderImage* image = nullptr;
							if (!TryGetOrAddImage(renderer, formatRenderTextureLookup, m_renderTextures, imageInfoLookup,
								inputInfo.Format, inputInfo.IsRead, false, requestedExtents, &image))
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
								bool matchingIsRead = indices[matchingInputInfo->second].Read;
								if (!matchingIsRead && inputImage->GetFormat() == outputInfo.Format && inputImage->GetDimensions() == requestedExtents)
								{
									image = inputImage; // Passthrough
								}
							}
						}

						if (image == nullptr && !TryGetOrAddImage(renderer, formatRenderTextureLookup, m_renderTextures, imageInfoLookup,
							outputInfo.Format, false, true, requestedExtents, &image))
							return false;

						node.OutputImages[info.first] = image;
					}

					const auto& inputBuffers = node.Node->GetBufferInputs();
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

					std::vector<std::unique_ptr<ICommandBuffer>> commandBuffers = std::move(m_renderCommandPool->CreateCommandBuffers(node.Node->GetName(), device, concurrentFrameCount));
					if (commandBuffers.empty())
					{
						return false;
					}

					m_renderCommandBuffers[pass] = std::move(commandBuffers);
				}
				else if (node.Type == RenderNodeType::Compute)
				{
					IComputePass* pass = static_cast<IComputePass*>(node.Node);
					if (!pass->Build(renderer, node.InputImages, node.OutputImages, node.InputBuffers, node.OutputBuffers))
					{
						Logger::Error("Failed to build compute pass '{}' while building render graph.", pass->GetName());
						return false;
					}

					std::vector<std::unique_ptr<ICommandBuffer>> commandBuffers = std::move(m_computeCommandPool->CreateCommandBuffers(node.Node->GetName(), device, concurrentFrameCount));
					if (commandBuffers.empty())
					{
						return false;
					}

					m_computeCommandBuffers[pass] = std::move(commandBuffers);
				}
			}
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

	bool RenderGraph::DrawRenderPass(const Renderer& renderer, const RenderGraphNode& node, uint32_t frameIndex,
		const glm::uvec2& size, SubmitInfo& renderSubmitInfo, bool& stageHasRenderPasses) const
	{
		const IDevice& device = renderer.GetDevice();

		IRenderPass* pass = static_cast<IRenderPass*>(node.Node);
		const std::vector<std::unique_ptr<ICommandBuffer>>& passCommandBuffers = m_renderCommandBuffers.at(pass);

		const ICommandBuffer& commandBuffer = *passCommandBuffers[frameIndex];
		commandBuffer.Reset();

		if (!commandBuffer.Begin())
		{
			return false;
		}

		m_renderStats.Begin(commandBuffer, pass->GetName(), false);

		for (const auto& imageInput : node.InputImages)
		{
			imageInput.second->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);
		}

		const std::vector<AttachmentInfo>& colourAttachments = pass->GetColourAttachments();
		for (const auto& colourAttachment : colourAttachments)
		{
			colourAttachment.renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::ColorAttachment);
		}

		const std::optional<AttachmentInfo>& depthAttachment = pass->GetDepthAttachment();

		if (depthAttachment.has_value())
		{
			depthAttachment->renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::DepthStencilAttachment);
		}

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
		stageHasRenderPasses = true;

		commandBuffer.End();

		renderSubmitInfo.CommandBuffers.emplace_back(&commandBuffer);

		return true;
	}

	bool RenderGraph::DispatchComputePass(Renderer& renderer, const RenderGraphNode& node, uint32_t frameIndex,
		SubmitInfo& computeSubmitInfo, bool& stageHasComputePasses) const
	{
		IComputePass* pass = static_cast<IComputePass*>(node.Node);
		const std::vector<std::unique_ptr<ICommandBuffer>>& passCommandBuffers = m_computeCommandBuffers.at(pass);

		const ICommandBuffer& commandBuffer = *passCommandBuffers[frameIndex];
		commandBuffer.Reset();

		if (!commandBuffer.Begin())
		{
			return false;
		}

		m_renderStats.Begin(commandBuffer, pass->GetName(), true);

		pass->Dispatch(renderer, commandBuffer, frameIndex);

		m_renderStats.End(commandBuffer, true);
		stageHasComputePasses = true;

		commandBuffer.End();

		computeSubmitInfo.CommandBuffers.emplace_back(&commandBuffer);

		return true;
	}

	bool RenderGraph::BlitToSwapchain(Renderer& renderer, const IDevice& device, uint32_t frameIndex,
		std::vector<SubmitInfo>& renderSubmitInfos, std::vector<SubmitInfo>& computeSubmitInfos) const
	{
		m_blitCommandBuffers[frameIndex]->Reset();
		if (!m_blitCommandBuffers[frameIndex]->Begin())
		{
			return false;
		}

		IRenderImage* finalImage = m_finalNode->OutputImages.at("Output");

		const glm::uvec3& extents = finalImage->GetDimensions();
		const glm::uvec3 offset(extents.x, extents.y, extents.z);

		ImageBlit blit;
		blit.srcSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.srcOffsets = std::array<glm::uvec3, 2> { glm::uvec3(), offset };
		blit.dstSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.dstOffsets = std::array<glm::uvec3, 2> { glm::uvec3(), offset };

		IRenderImage& presentImage = renderer.GetPresentImage();
		finalImage->TransitionImageLayout(device, *m_blitCommandBuffers[frameIndex], ImageLayout::TransferSrc);
		presentImage.TransitionImageLayout(device, *m_blitCommandBuffers[frameIndex], ImageLayout::TransferDst);
		m_blitCommandBuffers[frameIndex]->BlitImage(*finalImage, presentImage, { blit }, Filter::Linear);
		presentImage.TransitionImageLayout(device, *m_blitCommandBuffers[frameIndex], ImageLayout::PresentSrc);

		m_blitCommandBuffers[frameIndex]->End();

		++m_renderSemaphore->Value;
		SubmitInfo& blitSubmitInfo = renderSubmitInfos.emplace_back();
		blitSubmitInfo.CommandBuffers.emplace_back(m_blitCommandBuffers[frameIndex].get());
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

		bool firstStageWithRenderPassesInGraph = true;
		bool firstStageWithComputePassesInGraph = true;
		for (const auto& stage : m_renderGraph)
		{
			SubmitInfo renderSubmitInfo;
			SubmitInfo computeSubmitInfo;

			bool stageHasRenderPasses = false;
			bool stageHasComputePasses = false;
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					if (!DrawRenderPass(renderer, node, frameIndex, size, renderSubmitInfo, stageHasRenderPasses))
						return false;
				}
				else if (node.Type == RenderNodeType::Compute)
				{
					if (!DispatchComputePass(renderer, node, frameIndex, computeSubmitInfo, stageHasComputePasses))
						return false;
				}
			}

			if (stageHasRenderPasses)
			{
				++m_renderSemaphore->Value;

				if (firstStageWithRenderPassesInGraph)
				{
					renderSubmitInfo.WaitSemaphores.emplace_back(m_renderSemaphore.get());
					renderSubmitInfo.WaitValues.emplace_back(m_renderSemaphore->Value - 1);
					renderSubmitInfo.Stages.emplace_back(MaterialStageFlags::ColorAttachmentOutput);
				}

				renderSubmitInfo.SignalSemaphores.emplace_back(m_renderSemaphore.get());
				renderSubmitInfo.SignalValues.emplace_back(m_renderSemaphore->Value);
				firstStageWithRenderPassesInGraph = false;
			}

			if (stageHasComputePasses)
			{
				++m_computeSemaphore->Value;

				if (firstStageWithComputePassesInGraph)
				{
					computeSubmitInfo.WaitSemaphores.emplace_back(m_computeSemaphore.get());
					computeSubmitInfo.WaitValues.emplace_back(m_computeSemaphore->Value - 1);
					computeSubmitInfo.Stages.emplace_back(MaterialStageFlags::ComputeShader);
				}

				computeSubmitInfo.SignalSemaphores.emplace_back(m_computeSemaphore.get());
				computeSubmitInfo.SignalValues.emplace_back(m_computeSemaphore->Value);
				firstStageWithComputePassesInGraph = false;
			}

			if (!renderSubmitInfo.CommandBuffers.empty())
				renderSubmitInfos.emplace_back(std::move(renderSubmitInfo));

			if (!computeSubmitInfo.CommandBuffers.empty())
			{
				// TODO: Sort out render-compute synchronization.
				//computeSubmitInfos.emplace_back(std::move(computeSubmitInfo));
				renderSubmitInfos.emplace_back(std::move(computeSubmitInfo));
			}
		}

		return BlitToSwapchain(renderer, device, frameIndex, renderSubmitInfos, computeSubmitInfos);
	}
}