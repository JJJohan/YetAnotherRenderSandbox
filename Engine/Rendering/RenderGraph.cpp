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
		, m_commandBuffers()
		, m_commandPool(nullptr)
		, m_semaphore(nullptr)
		, m_renderTextures()
		, m_renderStats(renderStats)
		, m_blitCommandBuffers()
	{
	}

	RenderGraph::~RenderGraph()
	{
		m_blitCommandBuffers.clear();
		m_commandBuffers.clear();
	}

	bool RenderGraph::Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, const IResourceFactory& resourceFactory, uint32_t concurrentFrameCount)
	{
		const QueueFamilyIndices& indices = physicalDevice.GetQueueFamilyIndices();

		m_semaphore = std::move(resourceFactory.CreateGraphicsSemaphore());
		if (!m_semaphore->Initialise(device))
		{
			return false;
		}

		m_commandPool = std::move(resourceFactory.CreateCommandPool());
		if (!m_commandPool->Initialise(physicalDevice, device, indices.GraphicsFamily.value(), CommandPoolFlags::Reset))
		{
			return false;
		}

		m_blitCommandBuffers = std::move(m_commandPool->CreateCommandBuffers(device, concurrentFrameCount));
		if (m_blitCommandBuffers.empty())
		{
			return false;
		}

		return true;
	}

	bool RenderGraph::AddPass(IRenderPass* renderPass, const IMaterialManager& materialManager)
	{
		if (m_renderNodeLookup.contains(renderPass->GetName()))
		{
			Logger::Error("Render pass with name '{}' already exists in the render graph.", renderPass->GetName());
			return false;
		}

		std::unordered_map<const char*, IBuffer*> inputBuffers(renderPass->GetBufferInputs().begin(), renderPass->GetBufferInputs().end());
		std::unordered_map<const char*, RenderPassImageInfo> inputImageInfos(renderPass->GetImageInputInfos().begin(), renderPass->GetImageInputInfos().end());

		for (const auto& bufferOutput : renderPass->GetBufferOutputs())
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

			m_bufferResourceNodeLookup[bufferOutput.first] = renderPass;
		}

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
		m_renderNodeLookup[renderPass->GetName()] = renderPass;
		return true;
	}

	void RenderGraph::SetPassEnabled(const char* passName, bool enabled)
	{
		auto pass = m_renderNodeLookup.find(passName);
		if (pass == m_renderNodeLookup.end() || pass->second->GetNodeType() != RenderNodeType::Pass)
		{
			Logger::Error("Pass '{}' not found in render graph.", passName);
			return;
		}

		static_cast<IRenderPass*>(pass->second)->SetEnabled(enabled);
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

		for (const auto& imageOutput : renderResource->GetImageOutputs())
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

	struct ImageInfo
	{
		bool Read;
		bool Write;
		IRenderImage& Image;

		ImageInfo(IRenderImage& image)
			: Image(image)
			, Read(false)
			, Write(false)
		{
		}
	};

	inline bool TryGetOrAddImage(const Renderer& renderer, std::unordered_map<Format, std::vector<ImageInfo>>& formatRenderTextureLookup,
		std::vector<std::unique_ptr<IRenderImage>>& renderTextures, std::unordered_map<IRenderImage*, uint32_t>& imageInfoLookup,
		Format format, bool read, bool write, const glm::uvec3& dimensions, IRenderImage** result)
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

		if (!image->Initialise(device, ImageType::e2D, format, dimensions, 1, 1, ImageTiling::Optimal,
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

	bool RenderGraph::Build(const Renderer& renderer)
	{
		const IDevice& device = renderer.GetDevice();
		uint32_t concurrentFrameCount = renderer.GetConcurrentFrameCount();

		std::unordered_map<const char*, RenderGraphNode&> renderGraphNodeLookup;
		std::unordered_map<Format, std::vector<ImageInfo>> formatRenderTextureLookup;
		std::unordered_map<IRenderImage*, uint32_t> imageInfoLookup;
		m_renderTextures.clear();
		m_renderGraph.clear();
		m_commandBuffers.clear();
		m_renderTextures.clear();

		std::unordered_map<const char*, RenderGraphNode*> availableBufferSources{};
		std::unordered_map<const char*, RenderGraphNode*> availableImageSources{};

		const IPhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();
		Format depthFormat = physicalDevice.GetDepthFormat();
		const ISwapChain& swapchain = renderer.GetSwapChain();
		glm::uvec3 defaultExtents = glm::uvec3(swapchain.GetExtent(), 1);
		Format swapchainFormat = swapchain.GetFormat();

		if (!m_renderResources.empty())
		{
			std::vector<RenderGraphNode>& stage = m_renderGraph.emplace_back(std::vector<RenderGraphNode>());

			for (auto& renderResource : m_renderResources)
			{
				RenderGraphNode& addedNode = stage.emplace_back(RenderGraphNode(renderResource));

				for (const auto& bufferOutput : renderResource->GetBufferOutputs())
					availableBufferSources.emplace(bufferOutput.first, &addedNode);

				for (const auto& imageOutput : renderResource->GetImageOutputs())
					availableImageSources.emplace(imageOutput.first, &addedNode);
			}
		}

		uint32_t enabledPasses = 0;
		std::vector<IRenderPass*> renderPassStack;
		renderPassStack.reserve(m_renderPasses.size());
		for (const auto& pass : m_renderPasses)
		{
			if (pass->GetEnabled())
			{
				pass->UpdatePlaceholderFormats(swapchainFormat, depthFormat);

				renderPassStack.emplace_back(pass);
				++enabledPasses;
			}
		}

		while (!renderPassStack.empty())
		{
			std::vector<RenderGraphNode>& stage = m_renderGraph.emplace_back(std::vector<RenderGraphNode>());

			// Per-stage available resources.
			std::unordered_map<const char*, RenderGraphNode*> stageAvailableBufferSources(availableBufferSources);
			std::unordered_map<const char*, RenderGraphNode*> stageAvailableImageSources(availableImageSources);

			// Attempt to satisfy all requirements for each remaining render pass. If all requirements are met for a pass, remove it from the stack.
			for (auto it = renderPassStack.begin(); it != renderPassStack.end();)
			{
				IRenderPass* renderPass = *it;
				RenderGraphNode node = RenderGraphNode(renderPass);
				bool satisfied = true;

				for (const auto& bufferInput : renderPass->GetBufferInputs())
				{
					const auto& search = stageAvailableBufferSources.find(bufferInput.first);
					if (search != stageAvailableBufferSources.end())
					{
						// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
						node.InputBufferSources.emplace(bufferInput.first, *search->second);
						for (const auto& bufferOutput : renderPass->GetBufferOutputs())
						{
							if (strcmp(bufferOutput.first, search->first) == 0)
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
					for (const auto& imageInputInfo : renderPass->GetImageInputInfos())
					{
						const auto& search = stageAvailableImageSources.find(imageInputInfo.first);
						if (search != stageAvailableImageSources.end())
						{
							// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
							node.InputImageSources.insert_or_assign(imageInputInfo.first, *search->second);
							for (const auto& imageOutput : renderPass->GetImageOutputInfos())
							{
								if (strcmp(imageOutput.first, search->first) == 0)
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
					it = renderPassStack.erase(it);
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
				for (const auto& bufferOutput : node.Pass->GetBufferOutputs())
					availableBufferSources.insert_or_assign(bufferOutput.first, &node);

				for (const auto& imageOutput : node.Pass->GetImageOutputInfos())
					availableImageSources.insert_or_assign(imageOutput.first, &node);
			}
		}

		if (!availableImageSources.contains("Output"))
		{
			Logger::Error("Render graph should contain an image entry named 'Output' to be presented at end of frame.");
			return false;
		}

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
				if (node.Type == RenderNodeType::Pass)
				{
					const auto& inputImageInfos = node.Pass->GetImageInputInfos();
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

					const auto& outputImageInfos = node.Pass->GetImageOutputInfos();
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
				}
				else
				{
					// Build resources before passes.
					if (!node.Resource->Build(renderer))
					{
						Logger::Error("Failed to build render resource '{}' while building render graph.", node.Resource->GetName());
						return false;
					}

					// Add available resource to the lookup.
					for (const auto& output : node.Resource->GetImageOutputs())
					{
						auto& imageMap = formatRenderTextureLookup[output.second->GetFormat()];
						node.OutputImages[output.first] = output.second;
						imageMap.emplace_back(ImageInfo(*output.second));
					}
				}
			}
		}

		// Build the actual passes.
		for (const auto& stage : m_renderGraph)
		{
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					if (!node.Pass->Build(renderer, node.InputImages, node.OutputImages))
					{
						Logger::Error("Failed to build render pass '{}' while building render graph.", node.Resource->GetName());
						return false;
					}

					std::vector<std::unique_ptr<ICommandBuffer>> commandBuffers = std::move(m_commandPool->CreateCommandBuffers(device, concurrentFrameCount));
					if (commandBuffers.empty())
					{
						return false;
					}

					m_commandBuffers[node.Pass] = std::move(commandBuffers);
				}
			}
		}

		// Find the last usage of the 'Output' image and mark it as the final render pass.
		m_finalNode = nullptr;
		for (auto it = m_renderGraph.rbegin(); it != m_renderGraph.rend(); ++it)
		{
			for (const auto& node : *it)
			{
				if (node.Pass != nullptr)
				{
					for (const auto& imageOutput : node.Pass->GetImageOutputInfos())
					{
						if (strcmp(imageOutput.first, "Output") == 0)
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

		if (!m_renderStats.Initialise(renderer.GetPhysicalDevice(), device, enabledPasses))
		{
			return false;
		}

		m_dirty = false;
		return true;
	}

	bool RenderGraph::Draw(Renderer& renderer, uint32_t frameIndex) const
	{
		const IDevice& device = renderer.GetDevice();
		const glm::uvec2& size = renderer.GetSwapChain().GetExtent();
		std::vector<SubmitInfo> submitInfos;

		bool firstStageWithPassesInGraph = true;
		for (const auto& stage : m_renderGraph)
		{
			SubmitInfo submitInfo;

			bool stageHasPasses = false;
			for (const auto& node : stage)
			{
				if (node.Type == RenderNodeType::Pass)
				{
					const std::vector<std::unique_ptr<ICommandBuffer>>& passCommandBuffers = m_commandBuffers.at(node.Pass);

					const ICommandBuffer& commandBuffer = *passCommandBuffers[frameIndex];
					commandBuffer.Reset();

					if (!commandBuffer.Begin())
					{
						return false;
					}

					for (const auto& imageInput : node.InputImages)
					{
						imageInput.second->TransitionImageLayout(device, commandBuffer, ImageLayout::ShaderReadOnly);
					}

					const std::vector<AttachmentInfo>& colourAttachments = node.Pass->GetColourAttachments();
					for (const auto& colourAttachment : colourAttachments)
					{
						colourAttachment.renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::ColorAttachment);
					}

					const std::optional<AttachmentInfo>& depthAttachment = node.Pass->GetDepthAttachment();

					if (depthAttachment.has_value())
					{
						depthAttachment->renderImage->TransitionImageLayout(device, commandBuffer, ImageLayout::DepthStencilAttachment);
					}

					m_renderStats.Begin(commandBuffer, node.Pass->GetName());

					glm::uvec2 passSize = size;
					node.Pass->GetCustomSize(passSize);

					node.Pass->PreDraw(device, commandBuffer, passSize, frameIndex, node.InputImages, node.OutputImages);

					uint32_t layerCount = node.Pass->GetLayerCount();
					for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
					{
						commandBuffer.BeginRendering(colourAttachments, depthAttachment, passSize, layerCount);

						node.Pass->Draw(device, commandBuffer, passSize, frameIndex, layerIndex);

						commandBuffer.EndRendering();
					}

					node.Pass->PostDraw(device, commandBuffer, passSize, frameIndex, node.InputImages, node.OutputImages);

					m_renderStats.End(commandBuffer);
					stageHasPasses = true;

					commandBuffer.End();

					submitInfo.CommandBuffers.emplace_back(&commandBuffer);
				}
			}

			if (stageHasPasses)
			{
				++m_semaphore->Value;

				if (firstStageWithPassesInGraph)
				{
					submitInfo.WaitSemaphores.emplace_back(m_semaphore.get());
					submitInfo.WaitValues.emplace_back(m_semaphore->Value - 1);
					submitInfo.Stages.emplace_back(MaterialStageFlags::ColorAttachmentOutput);
				}

				submitInfo.SignalSemaphores.emplace_back(m_semaphore.get());
				submitInfo.SignalValues.emplace_back(m_semaphore->Value);
				firstStageWithPassesInGraph = false;
			}

			if (!submitInfo.CommandBuffers.empty())
				submitInfos.emplace_back(std::move(submitInfo));
		}

		// Blit to swapchain image.

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
		m_blitCommandBuffers[frameIndex]->BlitImage(*finalImage, presentImage, {blit}, Filter::Linear);
		presentImage.TransitionImageLayout(device, *m_blitCommandBuffers[frameIndex], ImageLayout::PresentSrc);

		m_blitCommandBuffers[frameIndex]->End();

		++m_semaphore->Value;
		SubmitInfo& blitSubmitInfo = submitInfos.emplace_back();
		blitSubmitInfo.CommandBuffers.emplace_back(m_blitCommandBuffers[frameIndex].get());
		blitSubmitInfo.WaitSemaphores.emplace_back(m_semaphore.get());
		blitSubmitInfo.WaitValues.emplace_back(m_semaphore->Value - 1);
		blitSubmitInfo.Stages.emplace_back(MaterialStageFlags::Transfer);
		blitSubmitInfo.SignalSemaphores.emplace_back(m_semaphore.get());
		blitSubmitInfo.SignalValues.emplace_back(m_semaphore->Value);

		m_renderStats.FinaliseResults(renderer.GetPhysicalDevice(), device, m_renderResources);

		return renderer.Present(submitInfos);
	}
}