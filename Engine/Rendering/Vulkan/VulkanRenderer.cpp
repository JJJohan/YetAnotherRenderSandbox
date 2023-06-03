#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VulkanRenderer.hpp"
#include "Core/Colour.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "VulkanSceneManager.hpp"
#include "UI/Vulkan/VulkanUIManager.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "DescriptorPool.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"
#include "Surface.hpp"
#include "SwapChain.hpp"
#include "CommandPool.hpp"
#include "ImageView.hpp"
#include "RenderImage.hpp"
#include "ImageSampler.hpp"
#include "PipelineLayout.hpp"
#include "FrameInfoUniformBuffer.hpp"
#include "LightUniformBuffer.hpp"
#include "GBuffer.hpp"
#include "ShadowMap.hpp"
#include "PostProcessing.hpp"
#include "VulkanRenderStats.hpp"

#define DEFAULT_MAX_CONCURRENT_FRAMES 2

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace Engine::OS;
using namespace Engine::Logging;
using namespace Engine::UI;
using namespace Engine::UI::Vulkan;

namespace Engine::Rendering::Vulkan
{
	VulkanRenderer::VulkanRenderer(const Window& window, bool debug)
		: Renderer(window, debug)
		, m_instance()
		, m_Debug()
		, m_physicalDevice()
		, m_device()
		, m_surface()
		, m_swapChain()
		, m_renderCommandPool()
		, m_resourceCommandPool()
		, m_renderCommandBuffers()
		, m_shadowCommandBuffers()
		, m_presentCommandBuffers()
		, m_postProcessingCommandBuffers()
		, m_uiCommandBuffers()
		, m_sceneManager()
		, m_imageAvailableSemaphores()
		, m_renderFinishedSemaphores()
		, m_shadowFinishedSemaphores()
		, m_presentFinishedSemaphores()
		, m_postProcessingFinishedSemaphores()
		, m_uiFinishedSemaphores()
		, m_inFlightFences()
		, m_inFlightResources()
		, m_actionQueue()
		, m_lastWindowSize()
		, m_swapChainOutOfDate(false)
		, m_currentFrame(0)
		, m_allocator()
		, m_maxConcurrentFrames(DEFAULT_MAX_CONCURRENT_FRAMES)
		, m_frameInfoBuffers()
		, m_lightBuffers()
		, m_frameInfoBufferData()
		, m_lightBufferData()
		, m_resourceSubmitMutex()
		, m_gBuffer()
		, m_shadowMap()
		, m_postProcessing()
		, m_renderStats()
		, m_uiManager(std::make_unique<VulkanUIManager>(window, *this))
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
		if (!m_instance.get())
		{
			return;
		}

		Logger::Verbose("Shutting down Vulkan renderer...");

		const vk::Device& deviceImp = m_device->Get();
		deviceImp.waitIdle(); // Shutdown warrants stalling GPU pipeline.

		m_uiManager.reset();
		m_sceneManager.reset();
		m_postProcessing.reset();
		m_renderStats.reset();

		m_frameInfoBuffers.clear();
		m_lightBuffers.clear();
		m_frameInfoBufferData.clear();
		m_lightBufferData.clear();
		m_imageAvailableSemaphores.clear();
		m_renderFinishedSemaphores.clear();
		m_shadowFinishedSemaphores.clear();
		m_presentFinishedSemaphores.clear();
		m_postProcessingFinishedSemaphores.clear();
		m_uiFinishedSemaphores.clear();
		m_inFlightFences.clear();
		m_renderCommandBuffers.clear();
		m_shadowCommandBuffers.clear();
		m_presentCommandBuffers.clear();
		m_postProcessingCommandBuffers.clear();
		m_uiCommandBuffers.clear();

		m_shadowMap.reset();
		m_gBuffer.reset();
		m_renderCommandPool.reset();
		m_resourceCommandPool.reset();
		m_swapChain.reset();

		if (m_allocator != nullptr)
		{
			vmaDestroyAllocator(m_allocator);
			m_allocator = nullptr;
		}

		m_Debug.reset();
		m_device.reset();
		m_surface.reset();
		m_instance.reset();
	}

	void VulkanRenderer::UpdateFrameInfo()
	{
		const glm::vec2& size = m_window.GetSize();

		FrameInfoUniformBuffer* frameInfo = m_frameInfoBufferData[m_currentFrame];
		frameInfo->view = m_camera.GetView();
		frameInfo->viewPos = glm::vec4(m_camera.GetPosition(), 1.0f);
		frameInfo->debugMode = m_debugMode;
		frameInfo->prevViewProj = frameInfo->viewProj;
		frameInfo->viewSize = size;
		frameInfo->viewProj = m_camera.GetViewProjection();
		frameInfo->jitter = m_postProcessing->IsEnabled() ? m_postProcessing->GetTAAJitter(size) : glm::vec2();
	}

	bool VulkanRenderer::RecordRenderCommandBuffer(const vk::CommandBuffer& commandBuffer)
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording render command buffer.");
			return false;
		}

		m_gBuffer->TransitionImageLayouts(*m_device, commandBuffer, vk::ImageLayout::eColorAttachmentOptimal);
		m_gBuffer->TransitionDepthLayout(*m_device, commandBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		std::vector<vk::RenderingAttachmentInfo> colorAttachments = m_gBuffer->GetRenderAttachments();
		vk::RenderingAttachmentInfo depthAttachment = m_gBuffer->GetDepthAttachment();

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = m_swapChain->GetExtent();
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
		renderingInfo.pColorAttachments = colorAttachments.data();
		renderingInfo.pDepthAttachment = &depthAttachment;

		m_renderStats->Begin(commandBuffer);
		commandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		commandBuffer.setScissor(0, 1, &scissor);

		UpdateFrameInfo();

		// Update light buffer
		LightUniformBuffer* lightInfo = m_lightBufferData[m_currentFrame];
		lightInfo->sunLightColour = m_sunColour.GetVec4();
		lightInfo->sunLightIntensity = m_sunIntensity;
		lightInfo->sunLightDir = m_sunDirection;

		const uint32_t cascadeCount = m_shadowMap->GetCascadeCount();
		ShadowCascadeData cascadeData = m_shadowMap->UpdateCascades(m_camera, m_sunDirection);
		for (uint32_t i = 0; i < cascadeCount; ++i)
		{
			lightInfo->cascadeMatrices[i] = cascadeData.CascadeMatrices[i];
			lightInfo->cascadeSplits[i] = cascadeData.CascadeSplits[i];
		}

		m_sceneManager->Draw(commandBuffer, m_currentFrame);

		commandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		m_gBuffer->TransitionImageLayouts(*m_device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

		commandBuffer.end();

		return true;
	}

	bool VulkanRenderer::RecordShadowCommandBuffer(const vk::CommandBuffer& commandBuffer)
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording shadow command buffer.");
			return false;
		}

		vk::Extent3D extents = m_shadowMap->GetShadowImage(0).GetDimensions();
		float width = static_cast<float>(extents.width);
		float height = static_cast<float>(extents.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = vk::Extent2D(extents.width, extents.height);
		renderingInfo.layerCount = 1;

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		commandBuffer.setScissor(0, 1, &scissor);

		// TODO: single pass
		uint32_t cascadeCount = m_shadowMap->GetCascadeCount();
		for (uint32_t i = 0; i < cascadeCount; ++i)
		{
			RenderImage& shadowImage = m_shadowMap->GetShadowImage(i);
			shadowImage.TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::eDepthStencilAttachmentOptimal);

			vk::RenderingAttachmentInfo shadowAttachment = m_shadowMap->GetShadowAttachment(i);
			renderingInfo.pDepthAttachment = &shadowAttachment;

			m_renderStats->Begin(commandBuffer);
			commandBuffer.beginRendering(renderingInfo);

			m_sceneManager->DrawShadows(commandBuffer, m_currentFrame, i);

			commandBuffer.endRendering();
			m_renderStats->End(commandBuffer);

			shadowImage.TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		commandBuffer.end();

		return true;
	}

	bool VulkanRenderer::RecordPresentCommandBuffer(const vk::CommandBuffer& commandBuffer)
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording present command buffer.");
			return false;
		}

		RenderImage& outputImage = m_gBuffer->GetOutputImage();
		const ImageView& outputImageView = m_gBuffer->GetOutputImageView();
		outputImage.TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::eColorAttachmentOptimal);

		vk::ClearValue clearValue = vk::ClearValue(vk::ClearColorValue(m_clearColour.r, m_clearColour.g, m_clearColour.b, m_clearColour.a));

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.imageView = outputImageView.Get();
		colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachmentInfo.clearValue = clearValue;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = m_swapChain->GetExtent();
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachmentInfo;

		m_renderStats->Begin(commandBuffer);
		commandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		commandBuffer.setScissor(0, 1, &scissor);

		m_gBuffer->DrawFinalImage(commandBuffer, m_currentFrame);

		commandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		commandBuffer.end();

		return true;
	}

	bool VulkanRenderer::RecordPostProcessingCommandBuffer(const vk::CommandBuffer& commandBuffer, RenderImage& colorImage, const ImageView& imageView)
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording post processing command buffer.");
			return false;
		}

		const ImageView& taaPrevImageView = m_postProcessing->GetTAAPrevImageView();

		colorImage.TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::eColorAttachmentOptimal);
		m_postProcessing->TransitionTAAImageLayouts(*m_device, commandBuffer);
		m_gBuffer->GetOutputImage().TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
		m_gBuffer->TransitionDepthLayout(*m_device, commandBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.imageView = imageView.Get();
		colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
		colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

		vk::RenderingAttachmentInfo previousColorAttachmentInfo = colorAttachmentInfo;
		previousColorAttachmentInfo.imageView = taaPrevImageView.Get();

		std::array< vk::RenderingAttachmentInfo, 2> attachmentInfos =
		{
			colorAttachmentInfo,
			previousColorAttachmentInfo
		};

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = m_swapChain->GetExtent();
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size());
		renderingInfo.pColorAttachments = attachmentInfos.data();

		m_renderStats->Begin(commandBuffer);
		commandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		commandBuffer.setScissor(0, 1, &scissor);

		m_postProcessing->Draw(commandBuffer, m_currentFrame);

		commandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		m_postProcessing->BlitTAA(*m_device, commandBuffer);

		commandBuffer.end();

		return true;
	}


	bool VulkanRenderer::RecordUICommandBuffer(const vk::CommandBuffer& commandBuffer, RenderImage& colorImage, const ImageView& imageView)
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording UI command buffer.");
			return false;
		}

		colorImage.TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::eColorAttachmentOptimal);

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.imageView = imageView.Get();
		colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
		colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = m_swapChain->GetExtent();
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachmentInfo;

		m_renderStats->Begin(commandBuffer);
		commandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		commandBuffer.setScissor(0, 1, &scissor);

		m_uiManager->Draw(commandBuffer, width, height);

		commandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		colorImage.TransitionImageLayout(*m_device, commandBuffer, vk::ImageLayout::ePresentSrcKHR);

		commandBuffer.end();

		return true;
	}

	bool VulkanRenderer::SubmitResourceCommand(std::function<bool(const vk::CommandBuffer&, std::vector<std::unique_ptr<Buffer>>&)> command,
		std::optional<std::function<void()>> postAction)
	{
		ResourceCommandData resourceData = {};
		resourceData.commandBuffer = m_resourceCommandPool->BeginResourceCommandBuffer(*m_device);

		if (!command(resourceData.commandBuffer.get(), resourceData.buffers))
		{
			return false;
		}

		resourceData.commandBuffer->end();
		const vk::Queue& queue = m_device->GetGraphicsQueue();

		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &resourceData.commandBuffer.get();

		resourceData.fence = m_device->Get().createFenceUnique(vk::FenceCreateInfo());
		resourceData.postAction = postAction;

		// TODO: Avoid this if possible.
		const std::lock_guard<std::mutex> lock(m_resourceSubmitMutex);

		vk::Result submitResult = queue.submit(1, &submitInfo, resourceData.fence.get());
		if (submitResult != vk::Result::eSuccess)
		{
			return false;
		}

		m_inFlightResources.push_back(std::move(resourceData));
		return true;
	}

	bool VulkanRenderer::CreateSyncObjects()
	{
		const vk::Device& deviceImp = m_device->Get();

		vk::SemaphoreCreateInfo semaphoreInfo{};
		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

		m_imageAvailableSemaphores.reserve(m_maxConcurrentFrames);
		m_renderFinishedSemaphores.reserve(m_maxConcurrentFrames);
		m_shadowFinishedSemaphores.reserve(m_maxConcurrentFrames);
		m_presentFinishedSemaphores.reserve(m_maxConcurrentFrames);
		m_postProcessingFinishedSemaphores.reserve(m_maxConcurrentFrames);
		m_uiFinishedSemaphores.reserve(m_maxConcurrentFrames);
		m_inFlightFences.reserve(m_maxConcurrentFrames);
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vk::UniqueSemaphore imageAvailableSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueSemaphore renderFinishedSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueSemaphore shadowFinishedSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueSemaphore presentFinishedSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueSemaphore postProcessingFinishedSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueSemaphore uiFinishedSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueFence inFlightFence = deviceImp.createFenceUnique(fenceInfo);
			if (!imageAvailableSemaphore.get() || !renderFinishedSemaphore.get() || !shadowFinishedSemaphore.get() ||
				!presentFinishedSemaphore.get() || !inFlightFence.get() || !postProcessingFinishedSemaphore.get() ||
				!uiFinishedSemaphore.get())
			{
				Logger::Error("Failed to create synchronization objects.");
				return false;
			}

			m_imageAvailableSemaphores.push_back(std::move(imageAvailableSemaphore));
			m_renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
			m_shadowFinishedSemaphores.push_back(std::move(shadowFinishedSemaphore));
			m_presentFinishedSemaphores.push_back(std::move(presentFinishedSemaphore));
			m_postProcessingFinishedSemaphores.push_back(std::move(postProcessingFinishedSemaphore));
			m_uiFinishedSemaphores.push_back(std::move(uiFinishedSemaphore));
			m_inFlightFences.push_back(std::move(inFlightFence));
		}

		return true;
	}

	bool VulkanRenderer::CreateAllocator()
	{
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = m_physicalDevice->Get();
		allocatorInfo.device = m_device->Get();
		allocatorInfo.instance = m_instance->Get();
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

		VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);
		return result == VK_SUCCESS;
	}

	bool VulkanRenderer::CreateFrameInfoUniformBuffer()
	{
		vk::DeviceSize bufferSize = sizeof(FrameInfoUniformBuffer);

		m_frameInfoBuffers.resize(m_maxConcurrentFrames);
		m_frameInfoBufferData.resize(m_maxConcurrentFrames);

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			m_frameInfoBuffers[i] = std::make_unique<Buffer>(m_allocator);
			Buffer& buffer = *m_frameInfoBuffers[i];

			if (!buffer.Initialise(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, vk::SharingMode::eExclusive))
			{
				return false;
			}

			if (!buffer.GetMappedMemory(&m_frameInfoBufferData[i]))
			{
				return false;
			}
		}

		return true;
	}

	bool VulkanRenderer::CreateLightUniformBuffer()
	{
		vk::DeviceSize bufferSize = sizeof(LightUniformBuffer);

		m_lightBuffers.resize(m_maxConcurrentFrames);
		m_lightBufferData.resize(m_maxConcurrentFrames);

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			m_lightBuffers[i] = std::make_unique<Buffer>(m_allocator);
			Buffer& buffer = *m_lightBuffers[i];

			if (!buffer.Initialise(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_AUTO,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, vk::SharingMode::eExclusive))
			{
				return false;
			}

			if (!buffer.GetMappedMemory(&m_lightBufferData[i]))
			{
				return false;
			}
		}

		return true;
	}

	void VulkanRenderer::SetTemporalAAState(bool enabled)
	{
		Renderer::SetTemporalAAState(enabled);

		m_actionQueue.push([this, enabled]()
			{
				m_postProcessing->SetEnabled(enabled); // Ensure in-flight semaphores get signaled.
				return true;
			});
	}

	void VulkanRenderer::SetHDRState(bool enable)
	{
		bool prevHDRState = m_hdr;
		Renderer::SetHDRState(enable);
		if (prevHDRState == m_hdr)
		{
			return;
		}

		m_actionQueue.push([this]()
			{
				const glm::uvec2 size = m_window.GetSize();
				if (!RecreateSwapChain(size, true))
				{
					Logger::Error("Failed to recreate swapchain.");
					return false;
				}

				return true;
			});
	}

	bool VulkanRenderer::Initialise()
	{
		Logger::Verbose("Initialising Vulkan renderer...");

		auto startTime = std::chrono::high_resolution_clock::now();

		static vk::DynamicLoader dl;
		auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

		std::string title = m_window.GetTitle();
		const glm::uvec2 size = m_window.GetSize();

		m_instance = std::make_unique<Instance>();
		m_surface = std::make_unique<Surface>();
		m_physicalDevice = std::make_unique<PhysicalDevice>();
		m_device = std::make_unique<Device>();
		m_swapChain = std::make_unique<SwapChain>();
		m_resourceCommandPool = std::make_unique<CommandPool>();
		m_renderCommandPool = std::make_unique<CommandPool>();
		m_Debug = std::make_unique<Debug>();
		m_sceneManager = std::make_unique<VulkanSceneManager>(*this);
		m_gBuffer = std::make_unique<GBuffer>(m_maxConcurrentFrames);
		m_shadowMap = std::make_unique<ShadowMap>();
		m_postProcessing = std::make_unique<PostProcessing>(*m_gBuffer, m_maxConcurrentFrames);
		m_renderStats = std::make_unique<VulkanRenderStats>(*m_gBuffer, *m_shadowMap);

		if (!m_instance->Initialise(title, *m_Debug, m_debug)
			|| !m_surface->Initialise(*m_instance, m_window)
			|| !m_physicalDevice->Initialise(*m_instance, *m_surface))
		{
			return false;
		}

		vk::Format depthFormat = m_physicalDevice->FindDepthFormat();
		float maxAnisotoropic = m_physicalDevice->GetLimits().maxSamplerAnisotropy;

		if (!m_device->Initialise(*m_physicalDevice)
			|| !CreateAllocator()
			|| !m_swapChain->Initialise(*m_physicalDevice, *m_device, *m_surface, m_window, m_allocator, size, m_hdr)
			|| !m_resourceCommandPool->Initialise(*m_physicalDevice, *m_device, vk::CommandPoolCreateFlagBits::eTransient)
			|| !m_renderCommandPool->Initialise(*m_physicalDevice, *m_device, vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			|| !CreateSyncObjects()
			|| !CreateFrameInfoUniformBuffer()
			|| !CreateLightUniformBuffer()
			|| !m_shadowMap->Rebuild(*m_device, m_allocator, depthFormat)
			|| !m_gBuffer->Initialise(*m_physicalDevice, *m_device, m_allocator,
				depthFormat, size, m_frameInfoBuffers, m_lightBuffers, *m_shadowMap)
			|| !m_postProcessing->Initialise(*m_physicalDevice, *m_device, m_allocator,
				m_swapChain->GetFormat(), size))
		{
			return false;
		}

		const uint32_t renderPassCount = m_shadowMap->GetCascadeCount() + 4; // Shadow cascades, scene, resolve, TAA & UI
		if (!m_renderStats->Initialise(*m_physicalDevice, *m_device, renderPassCount))
		{
			return false;
		}

		// Initialise UI
		if (!m_uiManager->Initialise(m_instance->Get(), *this))
		{
			Logger::Error("Failed to initialise UI.");
			return false;
		}

		m_renderCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_renderCommandBuffers.empty())
		{
			return false;
		}

		m_shadowCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_shadowCommandBuffers.empty())
		{
			return false;
		}

		m_presentCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_presentCommandBuffers.empty())
		{
			return false;
		}

		m_postProcessingCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_postProcessingCommandBuffers.empty())
		{
			return false;
		}

		m_uiCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_uiCommandBuffers.empty())
		{
			return false;
		}

		if (!m_sceneManager->Initialise(m_gBuffer->GetImageFormats(), m_gBuffer->GetDepthFormat()))
		{
			return false;
		}

		m_lastWindowSize = m_window.GetSize();

		auto endTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
		Logger::Verbose("Renderer setup finished in {} seconds.", deltaTime);

		return true;
	}

	bool VulkanRenderer::RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines)
	{
		m_device->Get().waitIdle(); // Recreating swapchain warrants stalling GPU pipeline.

		m_lastWindowSize = size;
		m_swapChainOutOfDate = false;

		if (!m_swapChain->Initialise(*m_physicalDevice, *m_device, *m_surface, m_window, m_allocator, size, m_hdr))
		{
			return false;
		}

		if (!m_gBuffer->Rebuild(*m_physicalDevice, *m_device, m_allocator, size,
			m_frameInfoBuffers, m_lightBuffers, *m_shadowMap, rebuildPipelines))
		{
			return false;
		}

		if (!m_postProcessing->Rebuild(*m_physicalDevice, *m_device, m_allocator,
			m_swapChain->GetFormat(), size, rebuildPipelines))
		{
			return false;
		}

		if (rebuildPipelines)
		{
			if (!m_sceneManager->RebuildShader(*m_physicalDevice, *m_device, m_gBuffer->GetImageFormats(), m_gBuffer->GetDepthFormat()))
			{
				Logger::Error("Failed to rebuild scene manager shader.");
				return false;
			}

			if (!m_uiManager->Rebuild(m_instance->Get(), *this))
			{
				Logger::Error("Failed to recreate UI render backend.");
				return false;
			}
		}

		return true;
	}

	bool VulkanRenderer::Render()
	{
		const vk::Device& deviceImp = m_device->Get();
		vk::Queue graphicsQueue = m_device->GetGraphicsQueue();
		vk::Queue presentQueue = m_device->GetPresentQueue();
		const vk::PhysicalDeviceLimits& limits = m_physicalDevice->GetLimits();

		// Exhaust action queue between frames.
		while (!m_actionQueue.empty())
		{
			std::function<bool()> action = m_actionQueue.front();
			m_actionQueue.pop();

			if (!action())
			{
				Logger::Error("Queued render action failed, aborting render loop.");
				return false;
			}
		}

		// Clean up in-flight resources.
		for (size_t i = 0; i < m_inFlightResources.size();)
		{
			const ResourceCommandData& resourceData = m_inFlightResources[i];
			if (deviceImp.waitForFences(1, &resourceData.fence.get(), true, 0) == vk::Result::eSuccess)
			{
				if (resourceData.postAction.has_value())
				{
					resourceData.postAction.value()();
				}

				m_inFlightResources.erase(m_inFlightResources.begin() + i);
			}
			else
			{
				++i;
			}
		}

		// Skip rendering in minimised state.
		const glm::uvec2& windowSize = m_window.GetSize();
		if (windowSize.x == 0 || windowSize.y == 0)
			return true;

		if (m_swapChainOutOfDate || windowSize != m_lastWindowSize)
		{
			RecreateSwapChain(windowSize, false);
			return true;
		}

		if (deviceImp.waitForFences(1, &m_inFlightFences[m_currentFrame].get(), true, UINT64_MAX) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to wait for fences.");
			return false;
		}

		const vk::SwapchainKHR& swapchainImp = m_swapChain->Get();

		uint32_t imageIndex;
		vk::Result acquireNextImageResult = deviceImp.acquireNextImageKHR(swapchainImp, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame].get(), VK_NULL_HANDLE, &imageIndex);
		if (acquireNextImageResult == vk::Result::eErrorOutOfDateKHR)
		{
			m_swapChainOutOfDate = true;
			return true; // Restart rendering.
		}
		else if (acquireNextImageResult != vk::Result::eSuccess && acquireNextImageResult != vk::Result::eSuboptimalKHR)
		{
			Logger::Error("Failed to reset command buffer.");
			return false;
		}

		if (deviceImp.resetFences(1, &m_inFlightFences[m_currentFrame].get()) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to reset fences.");
			return false;
		}

		m_renderCommandBuffers[m_currentFrame]->reset();

		m_camera.Update(windowSize);

		std::vector<vk::SubmitInfo> submitInfos;

		if (!RecordRenderCommandBuffer(m_renderCommandBuffers[m_currentFrame].get()))
		{
			return false;
		}

		submitInfos.emplace_back(vk::SubmitInfo(0, nullptr, nullptr, 1, &m_renderCommandBuffers[m_currentFrame].get(), 1, &m_renderFinishedSemaphores[m_currentFrame].get()));

		m_shadowCommandBuffers[m_currentFrame]->reset();

		if (!RecordShadowCommandBuffer(m_shadowCommandBuffers[m_currentFrame].get()))
		{
			return false;
		}

		submitInfos.emplace_back(vk::SubmitInfo(0, nullptr, nullptr, 1, &m_shadowCommandBuffers[m_currentFrame].get(), 1, &m_shadowFinishedSemaphores[m_currentFrame].get()));

		m_presentCommandBuffers[m_currentFrame]->reset();

		if (!RecordPresentCommandBuffer(m_presentCommandBuffers[m_currentFrame].get()))
		{
			return false;
		}

		std::array<vk::PipelineStageFlags, 3> presentWaitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput };
		std::array<vk::Semaphore, 3> presentWaitSemaphores = { m_imageAvailableSemaphores[m_currentFrame].get(), m_renderFinishedSemaphores[m_currentFrame].get(), m_shadowFinishedSemaphores[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo(3, presentWaitSemaphores.data(), presentWaitStages.data(), 1, &m_presentCommandBuffers[m_currentFrame].get(), 1, &m_presentFinishedSemaphores[m_currentFrame].get()));

		RenderImage& colorImage = m_swapChain->GetSwapChainImage(imageIndex);
		const ImageView& imageView = m_swapChain->GetSwapChainImageView(imageIndex);

		m_postProcessingCommandBuffers[m_currentFrame]->reset();

		if (!RecordPostProcessingCommandBuffer(m_postProcessingCommandBuffers[m_currentFrame].get(), colorImage, imageView))
		{
			return false;
		}

		std::array<vk::PipelineStageFlags, 1> postProcessingWaitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		std::array<vk::Semaphore, 1> postProcessingWaitSemaphores = { m_presentFinishedSemaphores[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo(1, postProcessingWaitSemaphores.data(), postProcessingWaitStages.data(), 1,
			&m_postProcessingCommandBuffers[m_currentFrame].get(), 1, &m_postProcessingFinishedSemaphores[m_currentFrame].get()));

		m_uiCommandBuffers[m_currentFrame]->reset();

		if (!RecordUICommandBuffer(m_uiCommandBuffers[m_currentFrame].get(), colorImage, imageView))
		{
			return false;
		}

		std::array<vk::PipelineStageFlags, 1> uiWaitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		std::array<vk::Semaphore, 1> uiWaitSemaphores = { m_postProcessingFinishedSemaphores[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo(1, uiWaitSemaphores.data(), uiWaitStages.data(), 1,
			&m_uiCommandBuffers[m_currentFrame].get(), 1, &m_uiFinishedSemaphores[m_currentFrame].get()));

		graphicsQueue.submit(submitInfos, m_inFlightFences[m_currentFrame].get());

		m_renderStats->GetResults(*m_physicalDevice, *m_device);

		vk::PresentInfoKHR presentInfo(1, &m_uiFinishedSemaphores[m_currentFrame].get(), 1, &swapchainImp, &imageIndex);

		// Exceptions in the core render loop, oh my!
		// The premise is that these are likely to only occur during swapchain resizing and are truly 'exceptional'.
		try
		{
			vk::Result result = presentQueue.presentKHR(presentInfo);
		}
		catch (vk::OutOfDateKHRError)
		{
			m_swapChainOutOfDate = true;
			return true; // Restart rendering, don't increment current frame index.
		}
		catch (std::exception ex)
		{
			Logger::Error("Fatal exception during present call occured: {}", ex.what());
			return false;
		}

		m_currentFrame = (m_currentFrame + 1) % m_maxConcurrentFrames;
		return true;
	}
}