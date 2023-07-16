#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VulkanRenderer.hpp"
#include "Core/Colour.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "../Resources/GeometryBatch.hpp"
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
#include "../RenderResources/GBuffer.hpp"
#include "../RenderResources/ShadowMap.hpp"
#include "../PostProcessing.hpp"
#include "VulkanRenderStats.hpp"
#include "ResourceFactory.hpp"
#include "VulkanTypesInterop.hpp"
#include "Semaphore.hpp"

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
		, m_resourceCommandPool()
		, m_imageAvailableSemaphores()
		, m_inFlightFences()
		, m_inFlightResources()
		, m_pendingResources()
		, m_actionQueue()
		, m_swapChainOutOfDate(false)
		, m_allocator()
		, m_resourceSubmitMutex()
		, m_presentImageIndex(0)
	{
		m_maxConcurrentFrames = DEFAULT_MAX_CONCURRENT_FRAMES;
	}

	void VulkanRenderer::DestroyResources()
	{
		m_pendingResources.clear();
		m_inFlightResources.clear();
		m_imageAvailableSemaphores.clear();
		m_inFlightFences.clear();

		Renderer::DestroyResources();

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

	bool VulkanRenderer::SubmitResourceCommand(std::function<bool(const IDevice& device, const IPhysicalDevice& physicalDevice, const ICommandBuffer&,
		std::vector<std::unique_ptr<IBuffer>>&)> command, std::optional<std::function<void()>> postAction)
	{
		ResourceCommandData resourceData = {};
		resourceData.commandBuffer = std::move(m_resourceCommandPool->BeginResourceCommandBuffer(*m_device));

		if (!command(*m_device, *m_physicalDevice, *resourceData.commandBuffer, resourceData.buffers))
		{
			return false;
		}

		resourceData.commandBuffer->End();
		resourceData.postAction = postAction;

		m_pendingResources.emplace_back(std::move(resourceData));
		return true;
	}

	bool VulkanRenderer::CreateSyncObjects()
	{
		const vk::Device& deviceImp = static_cast<Device*>(m_device.get())->Get();

		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

		m_imageAvailableSemaphores.reserve(m_maxConcurrentFrames);
		m_inFlightFences.reserve(m_maxConcurrentFrames);
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vk::UniqueSemaphore imageAvailableSemaphore = deviceImp.createSemaphoreUnique({});
			vk::UniqueFence inFlightFence = deviceImp.createFenceUnique(fenceInfo);
			
			if (!imageAvailableSemaphore.get() || !inFlightFence.get())
			{
				Logger::Error("Failed to create synchronisation objects.");
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
		m_lastWindowSize = m_window.GetSize();

		m_instance = std::make_unique<Instance>();
		m_surface = std::make_unique<Surface>();
		m_physicalDevice = std::make_unique<PhysicalDevice>();
		m_device = std::make_unique<Device>();
		m_swapChain = std::make_unique<SwapChain>();
		m_resourceCommandPool = std::make_unique<CommandPool>();
		m_Debug = std::make_unique<Debug>();
		m_sceneGeometryBatch = std::make_unique<GeometryBatch>(*this);
		m_renderStats = std::make_unique<VulkanRenderStats>();
		m_materialManager = std::make_unique<PipelineManager>();
		m_resourceFactory = std::make_unique<ResourceFactory>(&m_allocator);
		m_uiManager = std::make_unique<VulkanUIManager>(m_window, *this);

		PhysicalDevice* vkPhysicalDevice = static_cast<PhysicalDevice*>(m_physicalDevice.get());
		Device* vkDevice = static_cast<Device*>(m_device.get());

		if (!m_instance->Initialise(title, *m_Debug, m_debug)
			|| !m_surface->Initialise(*m_instance, m_window)
			|| !vkPhysicalDevice->Initialise(*m_instance, *m_surface)
			|| !vkDevice->Initialise(*m_physicalDevice)
			|| !m_postProcessing->Rebuild(m_lastWindowSize))
		{
			return false;
		}

		const QueueFamilyIndices& indices = vkPhysicalDevice->GetQueueFamilyIndices();

		if (!CreateAllocator()
			|| !static_cast<SwapChain*>(m_swapChain.get())->Initialise(*m_physicalDevice, *m_device, *m_surface, m_window, m_allocator, m_lastWindowSize, m_renderSettings.m_hdr)
			|| !m_resourceCommandPool->Initialise(*m_physicalDevice, *m_device, indices.GraphicsFamily.value(), CommandPoolFlags::Transient)
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

		auto endTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
		Logger::Verbose("Renderer setup finished in {} seconds.", deltaTime);

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

		if (!m_postProcessing->Rebuild(size))
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

		m_renderGraph->MarkDirty();

		return true;
	}

	IRenderImage& VulkanRenderer::GetPresentImage() const
	{
		return m_swapChain->GetSwapChainImage(m_presentImageIndex);
	}

	bool VulkanRenderer::Present(const std::vector<SubmitInfo>& submitInfos)
	{
		Device* vkDevice = static_cast<Device*>(m_device.get());
		const vk::Device& deviceImp = vkDevice->Get();
		const vk::SwapchainKHR& swapchainImp = static_cast<SwapChain*>(m_swapChain.get())->Get();

		vk::Queue graphicsQueue = vkDevice->GetGraphicsQueue();
		vk::Queue presentQueue = vkDevice->GetPresentQueue();

		std::vector<vk::SubmitInfo> vkSubmitInfos;
		std::vector<std::vector<vk::PipelineStageFlags>> stageArrays;
		std::vector<std::vector<vk::Semaphore>> waitSemaphoreArrays;
		std::vector<std::vector<vk::Semaphore>> signalSemaphoreArrays;
		std::vector<std::vector<vk::CommandBuffer>> commandBufferArrays;
		std::vector<vk::TimelineSemaphoreSubmitInfo> timelineInfos;
		uint32_t count = 0;

		for (const auto& submitInfo : submitInfos)
		{
			if (submitInfo.Stages.size() != submitInfo.WaitSemaphores.size())
			{
				Logger::Error("Submit info stage mask count ({}) does not match wait semaphore count ({}).", submitInfo.Stages.size(), submitInfo.WaitSemaphores.size());
				return false;
			}

			if (submitInfo.SignalSemaphores.size() != submitInfo.SignalValues.size())
			{
				Logger::Error("Signal semaphore count ({}) does not match signal value count ({}).", submitInfo.SignalSemaphores.size(), submitInfo.SignalValues.size());
				return false;
			}

			if (submitInfo.WaitSemaphores.size() != submitInfo.WaitValues.size())
			{
				Logger::Error("Wait semaphore count ({}) does not match wait value count ({}).", submitInfo.WaitSemaphores.size(), submitInfo.WaitValues.size());
				return false;
			}

			std::vector<vk::PipelineStageFlags>& stages = stageArrays.emplace_back();
			for (MaterialStageFlags stage : submitInfo.Stages)
			{
				stages.emplace_back(static_cast<vk::PipelineStageFlagBits>(stage));
			}

			std::vector<vk::CommandBuffer>& commandBuffers = commandBufferArrays.emplace_back();
			for (const ICommandBuffer* commandBuffer : submitInfo.CommandBuffers)
			{
				commandBuffers.emplace_back((static_cast<const CommandBuffer*>(commandBuffer))->Get());
			}

			std::vector<vk::Semaphore>& waitSemaphores = waitSemaphoreArrays.emplace_back();
			for (const ISemaphore* semaphore : submitInfo.WaitSemaphores)
			{
				waitSemaphores.emplace_back((static_cast<const Semaphore*>(semaphore))->Get());
			}

			std::vector<vk::Semaphore>& signalSemaphores = signalSemaphoreArrays.emplace_back();
			for (const ISemaphore* semaphore : submitInfo.SignalSemaphores)
			{
				signalSemaphores.emplace_back((static_cast<const Semaphore*>(semaphore))->Get());
			}

			timelineInfos.emplace_back(vk::TimelineSemaphoreSubmitInfo(submitInfo.WaitValues, submitInfo.SignalValues));
			++count;
		}

		vkSubmitInfos.resize(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			vkSubmitInfos[i] = vk::SubmitInfo(waitSemaphoreArrays[i], stageArrays[i], commandBufferArrays[i], signalSemaphoreArrays[i], &timelineInfos[i]);
		}

		graphicsQueue.submit(vkSubmitInfos, m_inFlightFences[m_currentFrame].get());

		vk::PresentInfoKHR presentInfo(1, &m_imageAvailableSemaphores[m_currentFrame].get(), 1, &swapchainImp, &m_presentImageIndex);
		vk::Result result = presentQueue.presentKHR(presentInfo);

		m_currentFrame = (m_currentFrame + 1) % m_maxConcurrentFrames;
		return result == vk::Result::eSuccess;
	}

	bool VulkanRenderer::Render()
	{
		Device* vkDevice = static_cast<Device*>(m_device.get());
		PhysicalDevice* vkPhysicalDevice = static_cast<PhysicalDevice*>(m_physicalDevice.get());

		const vk::Device& deviceImp = vkDevice->Get();
		vk::Queue graphicsQueue = vkDevice->GetGraphicsQueue();
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

					// Rebuild render graph when new resources are built. Might want to revisit..
					m_renderGraph->MarkDirty();
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
				commandBuffers[i] = static_cast<CommandBuffer*>(m_pendingResources[i].commandBuffer.get())->Get();
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

		bool materialManagerDirty = static_cast<PipelineManager*>(m_materialManager.get())->CheckDirty();
		bool renderGraphDirty = m_renderGraph->CheckDirty();

		if (materialManagerDirty || renderGraphDirty)
		{
			// Wait for both fences.
			vk::Fence nextFence = m_inFlightFences[(m_currentFrame + 1) % m_maxConcurrentFrames].get();
			if (deviceImp.waitForFences(1, &nextFence, true, UINT64_MAX) != vk::Result::eSuccess)
			{
				Logger::Error("Failed to wait for fences.");
				return false;
			}

			if (renderGraphDirty)
			{
				if (!m_renderGraph->Build(*this))
				{
					Logger::Error("Failed to build render graph.");
					return false;
				}

				materialManagerDirty = true;
			}

			if (materialManagerDirty && !m_materialManager->Update(*m_physicalDevice, *m_device, m_swapChain->GetFormat(), m_depthFormat))
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
			Logger::Error("Failed to acquire next swapchain image.");
			return false;
		}

		m_presentImageIndex = acquireNextImageResult.value;

		if (deviceImp.resetFences(1, &m_inFlightFences[m_currentFrame].get()) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to reset fence.");
			return false;
		}

		if (!Renderer::Render())
		{
			return false;
		}

		return true;
	}
}