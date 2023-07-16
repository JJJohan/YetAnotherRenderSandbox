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
		, m_renderPassLookup()
		, m_dirty(true)
		, m_finalPass(nullptr)
		, m_finalImage(nullptr)
		, m_commandBuffers()
		, m_commandPool(nullptr)
		, m_semaphore(nullptr)
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
		if (m_renderPassLookup.contains(renderPass->GetName()))
		{
			Logger::Error("Render pass with name '{}' already exists in the render graph.", renderPass->GetName());
			return false;
		}

		std::unordered_map<const char*, IBuffer*> inputBuffers(renderPass->GetBufferInputs().begin(), renderPass->GetBufferInputs().end());
		std::unordered_map<const char*, IRenderImage*> inputImages(renderPass->GetImageInputs().begin(), renderPass->GetImageInputs().end());

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

		for (const auto& imageOutput : renderPass->GetImageOutputs())
		{
			if (m_imageResourceNodeLookup.contains(imageOutput.first))
			{
				if (inputImages.contains(imageOutput.first))
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
		m_renderPassLookup[renderPass->GetName()] = renderPass;
		return true;
	}

	void RenderGraph::SetPassEnabled(const char* passName, bool enabled)
	{
		auto pass = m_renderPassLookup.find(passName);
		if (pass == m_renderPassLookup.end())
		{
			Logger::Error("Pass '{}' not found in render graph.", passName);
			return;
		}

		pass->second->SetEnabled(enabled);
		m_dirty = true;
	}

	bool RenderGraph::AddResource(IRenderResource* renderResource)
	{
		if (m_renderResources.contains(renderResource->GetName()))
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

		m_renderResources[renderResource->GetName()] = renderResource;
		return true;
	}

	bool RenderGraph::Build(const Renderer& renderer)
	{
		const IDevice& device = renderer.GetDevice();
		uint32_t concurrentFrameCount = renderer.GetConcurrentFrameCount();

		m_renderGraph.clear();
		m_commandBuffers.clear();

		std::unordered_map<const char*, const IRenderResource*> availableBufferSources{};
		std::unordered_map<const char*, const IRenderResource*> availableImageSources{};

		if (!m_renderResources.empty())
		{
			std::vector<RenderNode>& stage = m_renderGraph.emplace_back(std::vector<RenderNode>());

			for (auto& renderResource : m_renderResources)
			{
				for (const auto& bufferOutput : renderResource.second->GetBufferOutputs())
					availableBufferSources[bufferOutput.first] = renderResource.second;

				for (const auto& imageOutput : renderResource.second->GetImageOutputs())
					availableImageSources[imageOutput.first] = renderResource.second;

				RenderNode node(renderResource.second);
				stage.emplace_back(node);
			}
		}

		uint32_t enabledPasses = 0;
		std::vector<IRenderPass*> renderPassStack;
		renderPassStack.reserve(m_renderPasses.size());
		for (const auto& pass : m_renderPasses)
		{
			if (pass->GetEnabled())
			{
				renderPassStack.emplace_back(pass);
				++enabledPasses;
			}
		}

		while (!renderPassStack.empty())
		{
			std::vector<RenderNode>& stage = m_renderGraph.emplace_back(std::vector<RenderNode>());

			// Per-stage available resources.
			std::unordered_map<const char*, const IRenderResource*> stageAvailableBufferSources(availableBufferSources);
			std::unordered_map<const char*, const IRenderResource*> stageAvailableImageSources(availableImageSources);

			// Attempt to satisfy all requirements for each remaining render pass. If all requirements are met for a pass, remove it from the stack.
			for (auto it = renderPassStack.begin(); it != renderPassStack.end();)
			{
				IRenderPass* renderPass = *it;
				RenderNode node(renderPass);
				bool satisfied = true;

				for (const auto& bufferInput : renderPass->GetBufferInputs())
				{
					const auto& search = stageAvailableBufferSources.find(bufferInput.first);
					if (search != stageAvailableBufferSources.end())
					{
						// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
						node.InputBuffers[bufferInput.first] = search->second;
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
					for (const auto& imageInput : renderPass->GetImageInputs())
					{
						const auto& search = stageAvailableImageSources.find(imageInput.first);
						if (search != stageAvailableImageSources.end())
						{
							// Make resource unavailable for the rest of this stage if it's being written to (exists in output.)
							node.InputImages[imageInput.first] = search->second;
							for (const auto& imageOutput : renderPass->GetImageOutputs())
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
			for (const RenderNode& node : stage)
			{
				for (const auto& bufferOutput : node.Pass->GetBufferOutputs())
					availableBufferSources[bufferOutput.first] = node.Pass;

				for (const auto& imageOutput : node.Pass->GetImageOutputs())
					availableImageSources[imageOutput.first] = node.Pass;
			}
		}

		if (!availableImageSources.contains("Output"))
		{
			Logger::Error("Render graph should contain an image entry named 'Output' to be presented at end of frame.");
			return false;
		}

		// Build the actual passes.
		for (const auto& stage : m_renderGraph)
		{
			for (const auto& node : stage)
			{
				std::unordered_map<const char*, IRenderImage*> inputImages(node.InputImages.size());
				for (auto& inputImageSource : node.InputImages)
				{
					inputImages[inputImageSource.first] = inputImageSource.second->GetOutputImage(inputImageSource.first);
				}

				std::unordered_map<const char*, IBuffer*> inputBuffers(node.InputBuffers.size());
				for (auto& inputBufferSource : node.InputBuffers)
				{
					inputBuffers[inputBufferSource.first] = inputBufferSource.second->GetOutputBuffer(inputBufferSource.first);
				}

				if (!node.Resource->Build(renderer, inputImages, inputBuffers))
				{
					Logger::Error("Failed to build render resource '{}' while building render graph.", node.Resource->GetName());
					return false;
				}

				if (node.Pass != nullptr)
				{
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
		m_finalPass = nullptr;
		m_finalImage = nullptr;
		for (auto it = m_renderGraph.rbegin(); it != m_renderGraph.rend(); ++it)
		{
			for (const auto& node : *it)
			{
				if (node.Pass != nullptr)
				{
					for (const auto& imageOutput : node.Pass->GetImageOutputs())
					{
						if (strcmp(imageOutput.first, "Output") == 0)
						{
							m_finalPass = node.Pass;
							m_finalImage = imageOutput.second;
							break;
						}
					}
				}
			}

			if (m_finalPass != nullptr) 
				break;
		}

		if (m_finalPass == nullptr)
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
				if (node.Pass != nullptr)
				{
					const std::vector<std::unique_ptr<ICommandBuffer>>& passCommandBuffers = m_commandBuffers.at(node.Pass);

					const ICommandBuffer& commandBuffer = *passCommandBuffers[frameIndex];
					commandBuffer.Reset();

					if (!commandBuffer.Begin())
					{
						return false;
					}

					for (const auto& imageInput : node.Pass->GetImageInputs())
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

					node.Pass->PreDraw(device, commandBuffer, passSize, frameIndex);

					uint32_t layerCount = node.Pass->GetLayerCount();
					for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
					{
						commandBuffer.BeginRendering(colourAttachments, depthAttachment, passSize, layerCount);

						node.Pass->Draw(device, commandBuffer, passSize, frameIndex, layerIndex);

						commandBuffer.EndRendering();
					}

					node.Pass->PostDraw(device, commandBuffer, passSize, frameIndex);

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
		if (m_finalImage == nullptr)
		{
			return false;
		}

		m_blitCommandBuffers[frameIndex]->Reset();
		if (!m_blitCommandBuffers[frameIndex]->Begin())
		{
			return false;
		}

		const glm::uvec3& extents = m_finalImage->GetDimensions();
		const glm::uvec3 offset(extents.x, extents.y, extents.z);

		ImageBlit blit;
		blit.srcSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.srcOffsets = std::array<glm::uvec3, 2> { glm::uvec3(), offset };
		blit.dstSubresource = ImageSubresourceLayers(ImageAspectFlags::Color, 0, 0, 1);
		blit.dstOffsets = std::array<glm::uvec3, 2> { glm::uvec3(), offset };

		IRenderImage& presentImage = renderer.GetPresentImage();
		m_finalImage->TransitionImageLayout(device, *m_blitCommandBuffers[frameIndex], ImageLayout::TransferSrc);
		presentImage.TransitionImageLayout(device, *m_blitCommandBuffers[frameIndex], ImageLayout::TransferDst);
		m_blitCommandBuffers[frameIndex]->BlitImage(*m_finalImage, presentImage, {blit}, Filter::Linear);
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