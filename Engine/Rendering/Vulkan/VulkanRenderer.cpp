#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VulkanRenderer.hpp"
#include "Core/Colour.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "GeometryBatch.hpp"
#include "UI/Vulkan/VulkanUIManager.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"
#include "Surface.hpp"
#include "SwapChain.hpp"
#include "CommandPool.hpp"
#include "CommandBuffer.hpp"
#include "ImageView.hpp"
#include "ImageSampler.hpp"
#include "PipelineLayout.hpp"
#include "../RenderGraph.hpp"
#include "RenderImage.hpp"
#include "PipelineManager.hpp"
#include "../Resources/LightUniformBuffer.hpp"
#include "../GBuffer.hpp"
#include "../ShadowMap.hpp"
#include "../PostProcessing.hpp"
#include "VulkanRenderStats.hpp"
#include "ResourceFactory.hpp"
#include "VulkanTypesInterop.hpp"

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
		, m_surface()
		, m_renderCommandPool()
		, m_resourceCommandPool()
		, m_renderCommandBuffers()
		, m_shadowCommandBuffers()
		, m_combineCommandBuffers()
		, m_postProcessingCommandBuffers()
		, m_uiCommandBuffers()
		, m_imageAvailableSemaphores()
		, m_timelineSemaphore()
		, m_timelineValue(0)
		, m_inFlightFences()
		, m_inFlightResources()
		, m_pendingResources()
		, m_actionQueue()
		, m_swapChainOutOfDate(false)
		, m_allocator()
		, m_resourceSubmitMutex()
	{
		m_maxConcurrentFrames = DEFAULT_MAX_CONCURRENT_FRAMES;
	}

	void VulkanRenderer::DestroyResources()
	{
		Renderer::DestroyResources();

		m_pendingResources.clear();
		m_inFlightResources.clear();
		m_imageAvailableSemaphores.clear();
		m_timelineSemaphore.reset();
		m_inFlightFences.clear();
		m_renderCommandBuffers.clear();
		m_shadowCommandBuffers.clear();
		m_combineCommandBuffers.clear();
		m_postProcessingCommandBuffers.clear();
		m_uiCommandBuffers.clear();

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

	VulkanRenderer::~VulkanRenderer()
	{
		if (!m_instance.get())
		{
			return;
		}

		Logger::Verbose("Shutting down Vulkan renderer...");

		static_cast<PipelineManager*>(m_materialManager.get())->WritePipelineCache(*m_device);

		const vk::Device& deviceImp = static_cast<Device*>(m_device.get())->Get();
		deviceImp.waitIdle(); // Shutdown warrants stalling GPU pipeline.

		DestroyResources();
	}

	inline vk::Extent2D ToVulkanExtents(const glm::uvec2& extents)
	{
		return vk::Extent2D(extents.x, extents.y);
	}

	bool VulkanRenderer::RecordRenderCommandBuffer(const ICommandBuffer& commandBuffer)
	{
		vk::CommandBuffer vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();
		vk::CommandBufferBeginInfo beginInfo;
		if (vkCommandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording render command buffer.");
			return false;
		}

		m_gBuffer->TransitionImageLayouts(*m_device, commandBuffer, ImageLayout::ColorAttachment);
		m_gBuffer->TransitionDepthLayout(*m_device, commandBuffer, ImageLayout::DepthStencilAttachment);
		std::vector<AttachmentInfo> colorAttachments = m_gBuffer->GetRenderAttachments();
		vk::RenderingAttachmentInfo depthAttachment = GetAttachmentInfo(m_gBuffer->GetDepthAttachment());
		std::vector<vk::RenderingAttachmentInfo> vkColorAttachments(colorAttachments.size());
		for (size_t i = 0; i < colorAttachments.size(); ++i)
			vkColorAttachments[i] = GetAttachmentInfo(colorAttachments[i]);

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = ToVulkanExtents(m_swapChain->GetExtent());
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(vkColorAttachments.size());
		renderingInfo.pColorAttachments = vkColorAttachments.data();
		renderingInfo.pDepthAttachment = &depthAttachment;

		m_renderStats->Begin(commandBuffer);
		vkCommandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		vkCommandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		vkCommandBuffer.setScissor(0, 1, &scissor);

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

		m_sceneGeometryBatch->Draw(commandBuffer, m_currentFrame);

		vkCommandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		m_gBuffer->TransitionImageLayouts(*m_device, commandBuffer, ImageLayout::ShaderReadOnly);

		vkCommandBuffer.end();

		return true;
	}

	bool VulkanRenderer::RecordShadowCommandBuffer(const ICommandBuffer& commandBuffer)
	{
		vk::CommandBuffer vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();
		vk::CommandBufferBeginInfo beginInfo;
		if (vkCommandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording shadow command buffer.");
			return false;
		}

		const glm::uvec3& extents = m_shadowMap->GetShadowImage(0).GetDimensions();
		float width = static_cast<float>(extents.x);
		float height = static_cast<float>(extents.y);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		vkCommandBuffer.setViewport(0, 1, &viewport);

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = vk::Extent2D(extents.x, extents.y);
		renderingInfo.layerCount = 1;

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		vkCommandBuffer.setScissor(0, 1, &scissor);

		// TODO: single pass?
		uint32_t cascadeCount = m_shadowMap->GetCascadeCount();
		for (uint32_t i = 0; i < cascadeCount; ++i)
		{
			IRenderImage& shadowImage = m_shadowMap->GetShadowImage(i);
			shadowImage.TransitionImageLayout(*m_device, commandBuffer, ImageLayout::DepthStencilAttachment);

			vk::RenderingAttachmentInfo shadowAttachment = GetAttachmentInfo(m_shadowMap->GetShadowAttachment(i));
			renderingInfo.pDepthAttachment = &shadowAttachment;

			m_renderStats->Begin(commandBuffer);
			vkCommandBuffer.beginRendering(renderingInfo);

			m_sceneGeometryBatch->DrawShadows(commandBuffer, m_currentFrame, i);

			vkCommandBuffer.endRendering();
			m_renderStats->End(commandBuffer);

			shadowImage.TransitionImageLayout(*m_device, commandBuffer, ImageLayout::ShaderReadOnly);
		}

		vkCommandBuffer.end();

		return true;
	}

	bool VulkanRenderer::RecordPresentCommandBuffer(const ICommandBuffer& commandBuffer)
	{
		vk::CommandBuffer vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();
		vk::CommandBufferBeginInfo beginInfo;
		if (vkCommandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording present command buffer.");
			return false;
		}

		IRenderImage& outputImage = m_gBuffer->GetOutputImage();
		const IImageView& outputImageView = m_gBuffer->GetOutputImageView();
		outputImage.TransitionImageLayout(*m_device, commandBuffer, ImageLayout::ColorAttachment);

		vk::ClearValue clearValue = vk::ClearValue(vk::ClearColorValue(m_clearColour.r, m_clearColour.g, m_clearColour.b, m_clearColour.a));

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.imageView = static_cast<const ImageView&>(outputImageView).Get();
		colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
		colorAttachmentInfo.clearValue = clearValue;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = ToVulkanExtents(m_swapChain->GetExtent());
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachmentInfo;

		m_renderStats->Begin(commandBuffer);
		vkCommandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		vkCommandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		vkCommandBuffer.setScissor(0, 1, &scissor);

		m_gBuffer->DrawFinalImage(commandBuffer, m_currentFrame);

		vkCommandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		vkCommandBuffer.end();

		return true;
	}

	bool VulkanRenderer::RecordPostProcessingCommandBuffer(const ICommandBuffer& commandBuffer, IRenderImage& colorImage, const IImageView& imageView)
	{
		vk::CommandBuffer vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();
		vk::CommandBufferBeginInfo beginInfo;
		if (vkCommandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording post processing command buffer.");
			return false;
		}

		const IImageView& taaPrevImageView = m_postProcessing->GetTAAPrevImageView();

		colorImage.TransitionImageLayout(*m_device, commandBuffer, ImageLayout::ColorAttachment);
		m_postProcessing->TransitionTAAImageLayouts(*m_device, commandBuffer);
		m_gBuffer->GetOutputImage().TransitionImageLayout(*m_device, commandBuffer, ImageLayout::ShaderReadOnly);
		m_gBuffer->TransitionDepthLayout(*m_device, commandBuffer, ImageLayout::ShaderReadOnly);

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.imageView = static_cast<const ImageView&>(imageView).Get();
		colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
		colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

		vk::RenderingAttachmentInfo previousColorAttachmentInfo = colorAttachmentInfo;
		previousColorAttachmentInfo.imageView = static_cast<const ImageView&>(taaPrevImageView).Get();

		std::array<vk::RenderingAttachmentInfo, 2> attachmentInfos =
		{
			colorAttachmentInfo,
			previousColorAttachmentInfo
		};

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = ToVulkanExtents(m_swapChain->GetExtent());
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size());
		renderingInfo.pColorAttachments = attachmentInfos.data();

		m_renderStats->Begin(commandBuffer);
		vkCommandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		vkCommandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		vkCommandBuffer.setScissor(0, 1, &scissor);

		m_postProcessing->Draw(commandBuffer, m_currentFrame);

		vkCommandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		m_postProcessing->BlitTAA(*m_device, commandBuffer);

		vkCommandBuffer.end();

		return true;
	}

	void VulkanRenderer::SetDebugMode(uint32_t mode)
	{
		if (!m_gBuffer.get())
			return;

		m_gBuffer->SetDebugMode(mode);
		Renderer::SetDebugMode(mode);
	}

	bool VulkanRenderer::RecordUICommandBuffer(const ICommandBuffer& commandBuffer, IRenderImage& colorImage, const IImageView& imageView)
	{
		vk::CommandBuffer vkCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer).Get();
		vk::CommandBufferBeginInfo beginInfo;
		if (vkCommandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording UI command buffer.");
			return false;
		}

		colorImage.TransitionImageLayout(*m_device, commandBuffer, ImageLayout::ColorAttachment);

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.imageView = static_cast<const ImageView&>(imageView).Get();
		colorAttachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		colorAttachmentInfo.loadOp = vk::AttachmentLoadOp::eLoad;
		colorAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;

		vk::RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = vk::Offset2D();
		renderingInfo.renderArea.extent = ToVulkanExtents(m_swapChain->GetExtent());
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachmentInfo;

		m_renderStats->Begin(commandBuffer);
		vkCommandBuffer.beginRendering(renderingInfo);

		float width = static_cast<float>(renderingInfo.renderArea.extent.width);
		float height = static_cast<float>(renderingInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		vkCommandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderingInfo.renderArea.offset, renderingInfo.renderArea.extent);
		vkCommandBuffer.setScissor(0, 1, &scissor);

		m_uiManager->Draw(commandBuffer, width, height);

		vkCommandBuffer.endRendering();
		m_renderStats->End(commandBuffer);

		colorImage.TransitionImageLayout(*m_device, commandBuffer, ImageLayout::PresentSrc);

		vkCommandBuffer.end();

		return true;
	}

	bool VulkanRenderer::SubmitResourceCommand(std::function<bool(const IDevice& device, const IPhysicalDevice& physicalDevice, const ICommandBuffer&,
		std::vector<std::unique_ptr<IBuffer>>&)> command, std::optional<std::function<void()>> postAction)
	{
		ResourceCommandData resourceData = {};
		resourceData.commandBuffer = m_resourceCommandPool->BeginResourceCommandBuffer(*m_device);

		if (!command(*m_device, *m_physicalDevice, CommandBuffer(resourceData.commandBuffer.get()), resourceData.buffers))
		{
			return false;
		}

		resourceData.commandBuffer->end();
		resourceData.postAction = postAction;

		m_pendingResources.emplace_back(std::move(resourceData));
		return true;
	}

	bool VulkanRenderer::CreateSyncObjects()
	{
		const vk::Device& deviceImp = static_cast<Device*>(m_device.get())->Get();

		vk::SemaphoreTypeCreateInfo typeCreateInfo(vk::SemaphoreType::eTimeline);
		vk::SemaphoreCreateInfo timelineInfo({}, &typeCreateInfo);
		m_timelineSemaphore = deviceImp.createSemaphoreUnique(timelineInfo);

		if (!m_timelineSemaphore.get())
		{
			Logger::Error("Failed to create synchronization objects.");
			return false;
		}

		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

		m_imageAvailableSemaphores.reserve(m_maxConcurrentFrames);
		m_inFlightFences.reserve(m_maxConcurrentFrames);
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vk::UniqueSemaphore imageAvailableSemaphore = deviceImp.createSemaphoreUnique({});
			vk::UniqueFence inFlightFence = deviceImp.createFenceUnique(fenceInfo);
			if (!imageAvailableSemaphore.get() || !inFlightFence.get())
			{
				Logger::Error("Failed to create synchronization objects.");
				return false;
			}

			m_imageAvailableSemaphores.push_back(std::move(imageAvailableSemaphore));
			m_inFlightFences.push_back(std::move(inFlightFence));
		}

		return true;
	}

	bool VulkanRenderer::CreateAllocator()
	{
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = static_cast<PhysicalDevice*>(m_physicalDevice.get())->Get();
		allocatorInfo.device = static_cast<Device*>(m_device.get())->Get();
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
		bool prevHDRState = m_renderSettings.m_hdr;
		Renderer::SetHDRState(enable);
		if (prevHDRState == m_renderSettings.m_hdr)
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

		m_instance = std::make_unique<Instance>();
		m_surface = std::make_unique<Surface>();
		m_physicalDevice = std::make_unique<PhysicalDevice>();
		m_device = std::make_unique<Device>();
		m_swapChain = std::make_unique<SwapChain>();
		m_resourceCommandPool = std::make_unique<CommandPool>();
		m_renderCommandPool = std::make_unique<CommandPool>();
		m_Debug = std::make_unique<Debug>();
		m_sceneGeometryBatch = std::make_unique<GeometryBatch>(*this);
		m_renderStats = std::make_unique<VulkanRenderStats>(*m_gBuffer, *m_shadowMap);
		m_materialManager = std::make_unique<PipelineManager>();
		m_resourceFactory = std::make_unique<ResourceFactory>(&m_allocator);
		m_uiManager = std::make_unique<VulkanUIManager>(m_window, *this);

		PhysicalDevice* vkPhysicalDevice = static_cast<PhysicalDevice*>(m_physicalDevice.get());
		Device* vkDevice = static_cast<Device*>(m_device.get());

		if (!m_instance->Initialise(title, *m_Debug, m_debug)
			|| !m_surface->Initialise(*m_instance, m_window)
			|| !vkPhysicalDevice->Initialise(*m_instance, *m_surface)
			|| !vkDevice->Initialise(*m_physicalDevice))
		{
			return false;
		}

		const QueueFamilyIndices& indices = vkPhysicalDevice->GetQueueFamilyIndices();

		if (!CreateAllocator()
			|| !static_cast<SwapChain*>(m_swapChain.get())->Initialise(*m_physicalDevice, *m_device, *m_surface, m_window, m_allocator, m_lastWindowSize, m_renderSettings.m_hdr)
			|| !m_resourceCommandPool->Initialise(*m_physicalDevice, *m_device, indices.GraphicsFamily.value(), vk::CommandPoolCreateFlagBits::eTransient)
			|| !m_renderCommandPool->Initialise(*m_physicalDevice, *m_device, indices.GraphicsFamily.value(), vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			|| !CreateSyncObjects())
		{
			return false;
		}

		if (!Renderer::Initialise())
		{
			return false;
		}

		// Initialise UI
		if (!static_cast<VulkanUIManager*>(m_uiManager.get())->Initialise(m_instance->Get(), *this))
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

		m_combineCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_combineCommandBuffers.empty())
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

		auto endTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
		Logger::Verbose("Renderer setup finished in {} seconds.", deltaTime);

		return true;
	}

	bool VulkanRenderer::PrepareSceneGeometryBatch(IGeometryBatch** geometryBatch)
	{
		m_sceneGeometryBatch = std::make_unique<GeometryBatch>(*this);

		if (!static_cast<GeometryBatch*>(m_sceneGeometryBatch.get())->Initialise(*m_physicalDevice, *m_device, *m_resourceFactory, *m_materialManager))
		{
			return false;
		}

		*geometryBatch = m_sceneGeometryBatch.get();
		return true;
	}

	bool VulkanRenderer::RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines)
	{
		static_cast<Device*>(m_device.get())->Get().waitIdle(); // Recreating swapchain warrants stalling GPU pipeline.

		m_lastWindowSize = size;
		m_swapChainOutOfDate = false;

		if (!static_cast<SwapChain*>(m_swapChain.get())->Initialise(*m_physicalDevice, *m_device, *m_surface, m_window, m_allocator, size, m_renderSettings.m_hdr))
		{
			return false;
		}

		if (!m_gBuffer->Rebuild(*m_physicalDevice, *m_device, *m_resourceFactory, size,
			m_frameInfoBuffers, m_lightBuffers, *m_shadowMap))
		{
			return false;
		}

		if (!m_postProcessing->Rebuild(*m_physicalDevice, *m_device, *m_resourceFactory, size))
		{
			return false;
		}

		if (rebuildPipelines)
		{
			if (!static_cast<VulkanUIManager*>(m_uiManager.get())->Rebuild(m_instance->Get(), *this))
			{
				Logger::Error("Failed to recreate UI render backend.");
				return false;
			}
		}

		return true;
	}

	bool VulkanRenderer::Render()
	{
		Device* vkDevice = static_cast<Device*>(m_device.get());
		PhysicalDevice* vkPhysicalDevice = static_cast<PhysicalDevice*>(m_physicalDevice.get());

		const vk::Device& deviceImp = vkDevice->Get();
		vk::Queue graphicsQueue = vkDevice->GetGraphicsQueue();
		vk::Queue presentQueue = vkDevice->GetPresentQueue();
		vk::Queue computeQueue = vkDevice->GetComputeQueue();
		bool asyncCompute = vkDevice->AsyncCompute();
		const vk::PhysicalDeviceLimits& limits = vkPhysicalDevice->GetLimits();

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

		// Process in-flight resource requests.
		for (size_t i = 0; i < m_inFlightResources.size();)
		{
			const auto& resourceDataPair = m_inFlightResources[i];
			if (deviceImp.waitForFences(1, &resourceDataPair.first.get(), true, 0) == vk::Result::eSuccess)
			{
				for (const auto& resource : resourceDataPair.second)
				{
					if (resource.postAction.has_value())
					{
						resource.postAction.value()();
					}
				}

				m_inFlightResources.erase(m_inFlightResources.begin() + i);
			}
			else
			{
				++i;
			}
		}

		// Submit all currently pending resources.
		if (!m_pendingResources.empty())
		{
			std::vector<vk::CommandBuffer> commandBuffers(m_pendingResources.size());
			for (size_t i = 0; i < m_pendingResources.size(); ++i)
			{
				commandBuffers[i] = m_pendingResources[i].commandBuffer.get();
			}

			vk::SubmitInfo submitInfo;
			submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
			submitInfo.pCommandBuffers = commandBuffers.data();

			vk::UniqueFence fence = deviceImp.createFenceUnique(vk::FenceCreateInfo());

			vk::Result submitResult = graphicsQueue.submit(1, &submitInfo, fence.get());
			if (submitResult != vk::Result::eSuccess)
			{
				return false;
			}

			std::vector<ResourceCommandData> resources = std::move(m_pendingResources);
			m_inFlightResources.push_back(std::make_pair(std::move(fence), std::move(resources)));
			m_pendingResources.clear();
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

		if (static_cast<PipelineManager*>(m_materialManager.get())->CheckDirty())
		{
			// Wait for both fences.
			if (deviceImp.waitForFences(1, &m_inFlightFences[(m_currentFrame + 1) % m_maxConcurrentFrames].get(), true, UINT64_MAX) != vk::Result::eSuccess)
			{
				Logger::Error("Failed to wait for fences.");
				return false;
			}

			if (!m_materialManager->Update(*m_physicalDevice, *m_device, m_swapChain->GetFormat(), m_gBuffer->GetDepthFormat()))
			{
				Logger::Error("Failed to update pipeline manager.");
				return false;
			}
		}

		const vk::SwapchainKHR& swapchainImp = static_cast<SwapChain*>(m_swapChain.get())->Get();

		vk::ResultValue<uint32_t> acquireNextImageResult = deviceImp.acquireNextImageKHR(swapchainImp, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame].get());
		if (acquireNextImageResult.result == vk::Result::eErrorOutOfDateKHR)
		{
			m_swapChainOutOfDate = true;
			return true; // Restart rendering.
		}
		else if (acquireNextImageResult.result != vk::Result::eSuccess && acquireNextImageResult.result != vk::Result::eSuboptimalKHR)
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
		std::array<vk::Semaphore, 1> timelineSemaphore = { m_timelineSemaphore.get() };

		m_camera.Update(windowSize);

		std::vector<vk::SubmitInfo> submitInfos;

		if (!RecordRenderCommandBuffer(CommandBuffer(m_renderCommandBuffers[m_currentFrame].get())))
		{
			return false;
		}

		std::array<uint64_t, 1> renderIncrements = { ++m_timelineValue };
		vk::TimelineSemaphoreSubmitInfo renderTimelineInfo({}, renderIncrements);
		std::array<vk::CommandBuffer, 1> renderCommandBuffer = { m_renderCommandBuffers[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo({}, {}, renderCommandBuffer, timelineSemaphore, &renderTimelineInfo));

		m_shadowCommandBuffers[m_currentFrame]->reset();

		if (!RecordShadowCommandBuffer(CommandBuffer(m_shadowCommandBuffers[m_currentFrame].get())))
		{
			return false;
		}

		std::array<uint64_t, 1> shadowIncrements = { ++m_timelineValue };
		vk::TimelineSemaphoreSubmitInfo shadowTimelineInfo({}, shadowIncrements);
		std::array<vk::CommandBuffer, 1> shadowCommandBuffer = { m_shadowCommandBuffers[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo({}, {}, shadowCommandBuffer, timelineSemaphore, &shadowTimelineInfo));

		m_combineCommandBuffers[m_currentFrame]->reset();

		if (!RecordPresentCommandBuffer(CommandBuffer(m_combineCommandBuffers[m_currentFrame].get())))
		{
			return false;
		}

		std::array<uint64_t, 1> combineWaits = { m_timelineValue };
		std::array<uint64_t, 1> combineIncrements = { ++m_timelineValue };
		std::array<vk::PipelineStageFlags, 1> combineWaitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::TimelineSemaphoreSubmitInfo combineTimelineInfo(combineWaits, combineIncrements);
		std::array<vk::CommandBuffer, 1> combineCommandBuffer = { m_combineCommandBuffers[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo(timelineSemaphore, combineWaitStages, combineCommandBuffer, timelineSemaphore, &combineTimelineInfo));

		uint32_t imageIndex = acquireNextImageResult.value;
		IRenderImage& colorImage = m_swapChain->GetSwapChainImage(imageIndex);
		const IImageView& imageView = m_swapChain->GetSwapChainImageView(imageIndex);

		m_postProcessingCommandBuffers[m_currentFrame]->reset();

		if (!RecordPostProcessingCommandBuffer(CommandBuffer(m_postProcessingCommandBuffers[m_currentFrame].get()), colorImage, imageView))
		{
			return false;
		}

		std::array<uint64_t, 1> postProcessingWaits = { m_timelineValue };
		std::array<uint64_t, 1> postProcessIncrements = { ++m_timelineValue };
		std::array<vk::PipelineStageFlags, 1> postProcessWaitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::TimelineSemaphoreSubmitInfo postProcessTimelineInfo(postProcessingWaits, postProcessIncrements);
		std::array<vk::CommandBuffer, 1> postProcessCommandBuffer = { m_postProcessingCommandBuffers[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo(timelineSemaphore, postProcessWaitStages, postProcessCommandBuffer, timelineSemaphore, &postProcessTimelineInfo));

		m_uiCommandBuffers[m_currentFrame]->reset();

		if (!RecordUICommandBuffer(CommandBuffer(m_uiCommandBuffers[m_currentFrame].get()), colorImage, imageView))
		{
			return false;
		}

		std::array<uint64_t, 1> uiWaits = { m_timelineValue };
		std::array<uint64_t, 1> uiIncrements = { ++m_timelineValue };
		std::array<vk::PipelineStageFlags, 1> uiWaitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::TimelineSemaphoreSubmitInfo uiTimelineInfo(uiWaits, uiIncrements);
		std::array<vk::CommandBuffer, 1> uiCommandBuffer = { m_uiCommandBuffers[m_currentFrame].get() };
		submitInfos.emplace_back(vk::SubmitInfo(timelineSemaphore, uiWaitStages, uiCommandBuffer, timelineSemaphore, &uiTimelineInfo));

		graphicsQueue.submit(submitInfos, m_inFlightFences[m_currentFrame].get());

		m_renderStats->FinaliseResults(*m_physicalDevice, *m_device);

		vk::PresentInfoKHR presentInfo(1, &m_imageAvailableSemaphores[m_currentFrame].get(), 1, &swapchainImp, &imageIndex);
		vk::Result result = presentQueue.presentKHR(presentInfo);

		m_currentFrame = (m_currentFrame + 1) % m_maxConcurrentFrames;
		return result == vk::Result::eSuccess;
	}
}