#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "VulkanRenderer.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "PipelineLayout.hpp"
#include "VulkanMeshManager.hpp"
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

#define DEFAULT_MAX_CONCURRENT_FRAMES 2

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace Engine::OS;
using namespace Engine::Logging;

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
		, m_renderThread()
		, m_renderCommandBuffers()
		, m_meshManager()
		, m_imageAvailableSemaphores()
		, m_renderFinishedSemaphores()
		, m_inFlightFences()
		, m_actionQueue()
		, m_running(false)
		, m_swapChainOutOfDate(false)
		, m_currentFrame(0)
		, m_allocator()
		, m_maxConcurrentFrames(DEFAULT_MAX_CONCURRENT_FRAMES)
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
		if (!m_instance.get())
		{
			return;
		}

		Logger::Verbose("Shutting down Vulkan renderer...");

		m_actionQueue.clear();

		if (m_running)
		{
			m_running = false;
			m_renderThread.join();
		}

		const vk::Device& deviceImp = m_device->Get();
		deviceImp.waitIdle();

		m_meshManager.reset();

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

		m_meshManager->Draw(commandBuffer, renderPassInfo.renderArea.extent, m_currentFrame);

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
					return;
				}

				m_pipelineLayouts.erase(result);
			});
	}

	MeshManager* VulkanRenderer::GetMeshManager() const
	{
		return m_meshManager.get();
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
				Logger::Error("Failed to create synchronization objects for.");
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

	bool VulkanRenderer::Initialise()
	{
		Logger::Verbose("Initialising Vulkan renderer...");

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
		m_meshManager = std::make_unique<VulkanMeshManager>(m_maxConcurrentFrames);

		if (!m_instance->Initialise(title, *m_Debug, m_debug)
			|| !m_surface->Initialise(*m_instance, m_window)
			|| !m_physicalDevice->Initialise(*m_instance, *m_surface)
			|| !m_device->Initialise(*m_physicalDevice)
			|| !CreateAllocator()
			|| !m_swapChain->Initialise(*m_physicalDevice, *m_device, *m_surface, m_allocator, size)
			|| !m_renderPass->Initialise(*m_physicalDevice, *m_device, *m_swapChain)
			|| !m_swapChain->CreateFramebuffers(*m_device, *m_renderPass)
			|| !m_resourceCommandPool->Initialise(*m_physicalDevice, *m_device, vk::CommandPoolCreateFlagBits::eTransient)
			|| !m_renderCommandPool->Initialise(*m_physicalDevice, *m_device, vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
			|| !CreateSyncObjects())
		{
			return false;
		}

		m_renderCommandBuffers = m_renderCommandPool->CreateCommandBuffers(*m_device, m_maxConcurrentFrames);
		if (m_renderCommandBuffers.empty())
		{
			return false;
		}

		m_running = true;
		m_renderThread = std::thread([this]() { this->Render(); });

		return true;
	}

	bool VulkanRenderer::RecreateSwapChain(const glm::uvec2& size)
	{
		m_device->Get().waitIdle();

		m_swapChainOutOfDate = false;

		return m_swapChain->Initialise(*m_physicalDevice, *m_device, *m_surface, m_allocator, size)
			&& m_swapChain->CreateFramebuffers(*m_device, *m_renderPass);
	}

	void VulkanRenderer::Render()
	{
		const vk::Device& deviceImp = m_device->Get();
		vk::Queue graphicsQueue = m_device->GetGraphicsQueue();
		vk::Queue presentQueue = m_device->GetPresentQueue();
		float maxAnisotrophy = m_physicalDevice->GetMaxAnisotropy();

		while (m_running)
		{
			// Exhaust action queue between frames.
			std::function<void()> action;
			while (m_actionQueue.try_pop(action))
			{
				action();
			}

			if (!m_meshManager->Update(m_allocator, *m_device, *m_resourceCommandPool, maxAnisotrophy))
			{
				Logger::Error("Failed to update meshes.");
				return;
			}

			// Skip rendering in minimised state.
			const glm::uvec2& windowSize = m_window.GetSize();
			if (windowSize.x == 0 || windowSize.y == 0)
			{
				continue;
			}

			if (m_swapChainOutOfDate)
			{
				RecreateSwapChain(windowSize);
				continue;
			}

			if (deviceImp.waitForFences(1, &m_inFlightFences[m_currentFrame].get(), true, UINT64_MAX) != vk::Result::eSuccess)
			{
				Logger::Error("Failed to wait for fences.");
				return; // abort render loop on error.
			}

			const vk::SwapchainKHR& swapchainImp = m_swapChain->Get();

			uint32_t imageIndex;
			vk::Result acquireNextImageResult = deviceImp.acquireNextImageKHR(swapchainImp, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame].get(), VK_NULL_HANDLE, &imageIndex);
			if (acquireNextImageResult == vk::Result::eErrorOutOfDateKHR)
			{
				m_swapChainOutOfDate = true;
				continue; // Restart rendering.
			}
			else if (acquireNextImageResult != vk::Result::eSuccess && acquireNextImageResult != vk::Result::eSuboptimalKHR)
			{
				Logger::Error("Failed to reset command buffer.");
				return; // abort render loop on error.
			}

			if (deviceImp.resetFences(1, &m_inFlightFences[m_currentFrame].get()) != vk::Result::eSuccess)
			{
				Logger::Error("Failed to reset fences.");
				return; // abort render loop on error.
			}

			m_renderCommandBuffers[m_currentFrame]->reset();

			if (!RecordCommandBuffer(m_renderCommandBuffers[m_currentFrame].get(), imageIndex))
			{
				return; // abort render loop on error.
			}

			vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

			vk::SubmitInfo submitInfo(1, &m_imageAvailableSemaphores[m_currentFrame].get(), waitStages, 1, &m_renderCommandBuffers[m_currentFrame].get(), 1, &m_renderFinishedSemaphores[m_currentFrame].get());

			if (graphicsQueue.submit(1, &submitInfo, m_inFlightFences[m_currentFrame].get()) != vk::Result::eSuccess)
			{
				Logger::Error("Failed to submit draw command buffer.");
				return; // abort render loop on error.
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
				continue; // Restart rendering, don't increment current frame index.
			}
			catch (std::exception ex)
			{
				Logger::Error("Fatal exception during present call occured:", ex.what());
				return;
			}

			m_currentFrame = (m_currentFrame + 1) % m_maxConcurrentFrames;
		}
	}
}