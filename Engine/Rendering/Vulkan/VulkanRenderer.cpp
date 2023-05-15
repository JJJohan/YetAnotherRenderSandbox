#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VulkanRenderer.hpp"
#include "Core/Colour.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "PipelineLayout.hpp"
#include "VulkanSceneManager.hpp"
#include "UI/Vulkan/VulkanUIManager.hpp"
#include "../Shader.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "DescriptorPool.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"
#include "Surface.hpp"
#include "SwapChain.hpp"
#include "RenderPass.hpp"
#include "Framebuffer.hpp"
#include "CommandPool.hpp"
#include "ImageView.hpp"
#include "RenderImage.hpp"
#include "ImageSampler.hpp"
#include "FrameInfoUniformBuffer.hpp"
#include "../Shader.hpp"

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
		, m_renderPass()
		, m_renderCommandPool()
		, m_resourceCommandPool()
		, m_pipelineLayouts()
		, m_renderCommandBuffers()
		, m_sceneManager()
		, m_imageAvailableSemaphores()
		, m_renderFinishedSemaphores()
		, m_inFlightFences()
		, m_inFlightResources()
		, m_actionQueue()
		, m_lastWindowSize()
		, m_swapChainOutOfDate(false)
		, m_currentFrame(0)
		, m_allocator()
		, m_maxConcurrentFrames(DEFAULT_MAX_CONCURRENT_FRAMES)
		, m_frameInfoBuffers()
		, m_frameInfoBufferData()
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

		m_frameInfoBuffers.clear();
		m_frameInfoBufferData.clear();
		m_imageAvailableSemaphores.clear();
		m_renderFinishedSemaphores.clear();
		m_inFlightFences.clear();
		m_renderCommandBuffers.clear();
		m_pipelineLayouts.clear();

		m_renderCommandPool.reset();
		m_resourceCommandPool.reset();
		m_swapChain->DestroyFramebuffers(*m_device);
		m_renderPass.reset();
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

	Shader* VulkanRenderer::CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs)
	{
		std::unique_ptr<PipelineLayout>& pipelineLayout = m_pipelineLayouts.emplace_back(std::make_unique<PipelineLayout>());
		if (!pipelineLayout->Initialise(*m_device, name, programs, *m_renderPass))
		{
			return nullptr;
		}

		return static_cast<Shader*>(std::addressof(*pipelineLayout));
	}

	bool VulkanRenderer::RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, uint32_t imageIndex)
	{
		vk::CommandBufferBeginInfo beginInfo;
		if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to begin recording command buffer.");
			return false;
		}

		const std::vector<Framebuffer*> framebuffers = m_swapChain->GetFramebuffers();

		std::array<vk::ClearValue, 2> clearValues =
		{
			vk::ClearValue(vk::ClearColorValue(m_clearColour.r, m_clearColour.g, m_clearColour.b, m_clearColour.a)),
			vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0))
		};

		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = vk::StructureType::eRenderPassBeginInfo;
		renderPassInfo.renderPass = m_renderPass->Get();
		renderPassInfo.framebuffer = framebuffers[imageIndex]->Get();
		renderPassInfo.renderArea.offset = vk::Offset2D();
		renderPassInfo.renderArea.extent = m_swapChain->GetExtent();
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);


		float width = static_cast<float>(renderPassInfo.renderArea.extent.width);
		float height = static_cast<float>(renderPassInfo.renderArea.extent.height);
		vk::Viewport viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
		commandBuffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor(renderPassInfo.renderArea.offset, renderPassInfo.renderArea.extent);
		commandBuffer.setScissor(0, 1, &scissor);

		// Update uniform buffer
		FrameInfoUniformBuffer frameInfo{};
		frameInfo.viewProj = m_camera.GetViewProjection();
		frameInfo.viewPos = glm::vec4(m_camera.GetPosition(), 1.0f);
		frameInfo.ambientColour = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
		frameInfo.sunLightColour = m_sunColour.GetVec4();
		frameInfo.sunLightDir = glm::vec4(m_sunDirection, 1.0f);
		memcpy(m_frameInfoBufferData[m_currentFrame], &frameInfo, sizeof(FrameInfoUniformBuffer));

		m_sceneManager->Draw(commandBuffer, m_currentFrame);

		m_uiManager->Draw(commandBuffer, width, height);

		commandBuffer.endRenderPass();
		commandBuffer.end();

		return true;
	}

	void VulkanRenderer::DestroyShader(Shader* shader)
	{
		PipelineLayout* pipelineLayout = static_cast<PipelineLayout*>(shader);

		m_actionQueue.push([&, pipelineLayout]()
			{
				const auto& result = std::find_if(m_pipelineLayouts.begin(), m_pipelineLayouts.end(), [pipelineLayout](const std::unique_ptr<PipelineLayout>& s)
					{
						return std::addressof(*s) == pipelineLayout;
					});

				if (result == m_pipelineLayouts.end())
				{
					Logger::Error("Shader was not found.");
					return true; // not fatal
				}

				m_pipelineLayouts.erase(result);
				return true;
			});
	}

	SceneManager* VulkanRenderer::GetSceneManager() const
	{
		return m_sceneManager.get();
	}

	UIManager* VulkanRenderer::GetUIManager() const
	{
		return m_uiManager.get();
	}

	const Device& VulkanRenderer::GetDevice() const
	{
		return *m_device;
	}

	const PhysicalDevice& VulkanRenderer::GetPhysicalDevice() const
	{
		return *m_physicalDevice;
	}
	const RenderPass& VulkanRenderer::GetRenderPass() const
	{
		return *m_renderPass;
	}
	const SwapChain& VulkanRenderer::GetSwapChain() const
	{
		return *m_swapChain;
	}

	VmaAllocator VulkanRenderer::GetAllocator() const
	{
		return m_allocator;
	}

	uint32_t VulkanRenderer::GetConcurrentFrameCount() const
	{
		return m_maxConcurrentFrames;
	}

	const std::vector<std::unique_ptr<Buffer>>& VulkanRenderer::GetFrameInfoBuffers() const
	{
		return m_frameInfoBuffers;
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
		m_inFlightFences.reserve(m_maxConcurrentFrames);
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vk::UniqueSemaphore imageAvailableSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueSemaphore renderFinishedSemaphore = deviceImp.createSemaphoreUnique(semaphoreInfo);
			vk::UniqueFence inFlightFence = deviceImp.createFenceUnique(fenceInfo);
			if (!imageAvailableSemaphore.get() || !renderFinishedSemaphore.get() || !inFlightFence.get())
			{
				Logger::Error("Failed to create synchronization objects.");
				return false;
			}

			m_imageAvailableSemaphores.push_back(std::move(imageAvailableSemaphore));
			m_renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
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

	void VulkanRenderer::SetMultiSampleCount(uint32_t multiSampleCount)
	{
		uint32_t prevMultiSampleCount = m_multiSampleCount;
		Renderer::SetMultiSampleCount(multiSampleCount);
		if (m_multiSampleCount == prevMultiSampleCount)
		{
			return;
		}

		m_actionQueue.push([this]()
			{
				const glm::uvec2 size = this->m_window.GetSize();
				vk::SampleCountFlagBits multiSampleCount = GetMultiSampleCount(m_multiSampleCount);

				this->m_device->Get().waitIdle(); // Fence for present?

				if (!this->m_swapChain->Initialise(*this->m_physicalDevice, *this->m_device, *this->m_surface, this->m_allocator, size, multiSampleCount))
				{
					Logger::Error("Failed to recreate swapchain.");
					return false;
				}

				if (!this->m_renderPass->Initialise(*this->m_physicalDevice, *this->m_device, *this->m_swapChain, this->m_swapChain->GetSampleCount()))
				{
					Logger::Error("Failed to recreate render pass.");
					return false;
				}

				if (!this->m_swapChain->CreateFramebuffers(*this->m_device, *this->m_renderPass))
				{
					Logger::Error("Failed to recreate framebuffer.");
					return false;
				}

				for (auto& layout : this->m_pipelineLayouts)
				{
					if (!layout->Rebuild(*this->m_device, *this->m_renderPass, UINT32_MAX))
					{
						Logger::Error("Failed to rebuild pipeline layout '{}'.", layout->GetName());
						return false;
					}
				}

				if (!m_uiManager->Rebuild(this->m_device->Get(), this->m_renderPass->Get(), multiSampleCount))
				{
					Logger::Error("Failed to recreate UI render backend.");
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

		m_renderCommandBuffers.resize(m_maxConcurrentFrames);

		m_instance = std::make_unique<Instance>();
		m_surface = std::make_unique<Surface>();
		m_physicalDevice = std::make_unique<PhysicalDevice>();
		m_device = std::make_unique<Device>();
		m_swapChain = std::make_unique<SwapChain>();
		m_renderPass = std::make_unique<RenderPass>();
		m_resourceCommandPool = std::make_unique<CommandPool>();
		m_renderCommandPool = std::make_unique<CommandPool>();
		m_Debug = std::make_unique<Debug>();
		m_sceneManager = std::make_unique<VulkanSceneManager>(*this);

		if (!m_instance->Initialise(title, *m_Debug, m_debug)
			|| !m_surface->Initialise(*m_instance, m_window)
			|| !m_physicalDevice->Initialise(*m_instance, *m_surface)
			|| !m_device->Initialise(*m_physicalDevice)
			|| !CreateAllocator()
			|| !m_swapChain->Initialise(*m_physicalDevice, *m_device, *m_surface, m_allocator, size, GetMultiSampleCount(m_multiSampleCount))
			|| !m_renderPass->Initialise(*m_physicalDevice, *m_device, *m_swapChain, m_swapChain->GetSampleCount())
			|| !m_swapChain->CreateFramebuffers(*m_device, *m_renderPass)
			|| !m_resourceCommandPool->Initialise(*m_physicalDevice, *m_device, vk::CommandPoolCreateFlagBits::eTransient)
			|| !m_renderCommandPool->Initialise(*m_physicalDevice, *m_device, vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			|| !CreateSyncObjects()
			|| !CreateFrameInfoUniformBuffer())

		{
			return false;
		}

		// Initialise UI
		if (!m_uiManager->Initialise(m_instance->Get(), *this))
		{
			Logger::Error("Failed to initialise UI.");
			return false;
		}

		m_maxMultiSampleCount = MultiSampleCountToInteger(m_physicalDevice->GetMaxMultiSampleCount());

		m_renderCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_renderCommandBuffers.empty())
		{
			return false;
		}

		// TODO: Clean up!
		Shader* shader = Renderer::CreateShader("Triangle", {
			{ ShaderProgramType::VERTEX, "Shaders/Triangle_vert.spv" },
			{ ShaderProgramType::FRAGMENT, "Shaders/Triangle_frag.spv" }
			});

		if (shader == nullptr)
		{
			Logger::Error("Shader invalid.");
			return 1;
		}

		if (!m_sceneManager->Initialise(shader))
		{
			return false;
		}

		m_lastWindowSize = m_window.GetSize();

		auto endTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime).count();
		Logger::Verbose("Renderer setup finished in {} seconds.", deltaTime);

		return true;
	}

	vk::SampleCountFlagBits VulkanRenderer::GetMultiSampleCount(uint32_t multiSampleCount) const
	{
		switch (multiSampleCount)
		{
		case 64:
			return vk::SampleCountFlagBits::e64;
		case 32:
			return vk::SampleCountFlagBits::e32;
		case 16:
			return vk::SampleCountFlagBits::e16;
		case 8:
			return vk::SampleCountFlagBits::e8;
		case 4:
			return vk::SampleCountFlagBits::e4;
		case 2:
			return vk::SampleCountFlagBits::e2;
		default:
			return vk::SampleCountFlagBits::e1;
		}
	}

	uint32_t VulkanRenderer::MultiSampleCountToInteger(vk::SampleCountFlagBits multiSampleCount) const
	{
		switch (multiSampleCount)
		{
		case vk::SampleCountFlagBits::e64:
			return 64;
		case vk::SampleCountFlagBits::e32:
			return 32;
		case vk::SampleCountFlagBits::e16:
			return 16;
		case vk::SampleCountFlagBits::e8:
			return 8;
		case vk::SampleCountFlagBits::e4:
			return 4;
		case vk::SampleCountFlagBits::e2:
			return 2;
		default:
			return 1;
		}
	}

	bool VulkanRenderer::RecreateSwapChain(const glm::uvec2& size)
	{
		vk::SampleCountFlagBits multiSampleCount = GetMultiSampleCount(m_multiSampleCount);
		m_device->Get().waitIdle(); // Recreating swapchain warrants stalling GPU pipeline.

		m_lastWindowSize = size;
		m_swapChainOutOfDate = false;

		return m_swapChain->Initialise(*m_physicalDevice, *m_device, *m_surface, m_allocator, size, multiSampleCount)
			&& m_swapChain->CreateFramebuffers(*m_device, *m_renderPass);
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
			RecreateSwapChain(windowSize);
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

		if (!RecordCommandBuffer(m_renderCommandBuffers[m_currentFrame].get(), imageIndex))
		{
			return false;
		}

		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

		vk::SubmitInfo submitInfo(1, &m_imageAvailableSemaphores[m_currentFrame].get(), waitStages, 1, &m_renderCommandBuffers[m_currentFrame].get(), 1, &m_renderFinishedSemaphores[m_currentFrame].get());

		if (graphicsQueue.submit(1, &submitInfo, m_inFlightFences[m_currentFrame].get()) != vk::Result::eSuccess)
		{
			Logger::Error("Failed to submit draw command buffer.");
			return false;
		}

		vk::PresentInfoKHR presentInfo(1, &m_renderFinishedSemaphores[m_currentFrame].get(), 1, &swapchainImp, &imageIndex);

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