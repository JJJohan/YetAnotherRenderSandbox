#include "VulkanRenderer.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "PipelineLayout.hpp"

#define DEFAULT_MAX_CONCURRENT_FRAMES 2

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
		, m_commandPool()
		, m_pipelineLayouts()
		, m_renderThread()
		, m_commandBuffers()
		, m_imageAvailableSemaphores()
		, m_renderFinishedSemaphores()
		, m_inFlightFences()
		, m_running(false)
		, m_maxConcurrentFrames(DEFAULT_MAX_CONCURRENT_FRAMES)
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
		Shutdown();
	}

	void VulkanRenderer::Shutdown()
	{
		if (!m_instance.Get())
		{
			return;
		}

		Logger::Verbose("Shutting down Vulkan renderer...");

		vkDeviceWaitIdle(m_device.Get());

		if (m_running)
		{
			m_running = false;
			//m_renderThread.join();
		}

		if (m_debug)
		{
			m_Debug.RemoveDebugCallback(m_instance);
		}

		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			vkDestroySemaphore(m_device.Get(), m_imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(m_device.Get(), m_renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(m_device.Get(), m_inFlightFences[i], nullptr);
		}
		m_imageAvailableSemaphores.clear();
		m_renderFinishedSemaphores.clear();
		m_inFlightFences.clear();

		m_commandPool.Shutdown(m_device);

		for (auto& pipelineLayout : m_pipelineLayouts)
		{
			pipelineLayout.Destroy(m_device);
		}
		m_pipelineLayouts.clear();

		m_swapChain.ShutdownFramebuffers(m_device);
		m_renderPass.Shutdown(m_device);
		m_swapChain.Shutdown(m_device);
		m_surface.Shutdown(m_instance);
		m_device.Shutdown();
		m_instance.Shutdown();
	}

	Shader* VulkanRenderer::CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs)
	{
		if (m_device.Get() == nullptr)
		{
			Logger::Error("Could not create shader, graphics device does not exist.");
			return nullptr;
		}

		PipelineLayout& pipelineLayout = m_pipelineLayouts.emplace_back(PipelineLayout{});
		if (!pipelineLayout.Create(m_device, name, programs, m_renderPass))
		{
			return nullptr;
		}

		return &pipelineLayout;
	}

	bool VulkanRenderer::RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			Logger::Error("Failed to begin recording command buffer.");
			return false;
		}

		const std::vector<Framebuffer> framebuffers = m_swapChain.GetFramebuffers();

		VkClearValue clearColor = { m_clearColour.r, m_clearColour.g, m_clearColour.b, m_clearColour.a };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass.Get();
		renderPassInfo.framebuffer = framebuffers[imageIndex].Get();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapChain.GetExtent();
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(renderPassInfo.renderArea.extent.width);
		viewport.height = static_cast<float>(renderPassInfo.renderArea.extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = renderPassInfo.renderArea.offset;
		scissor.extent = renderPassInfo.renderArea.extent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		for (const auto& pipelineLayout : m_pipelineLayouts)
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.GetGraphicsPipeline());
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		}

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			Logger::Error("Failed to record command buffer.");
			return false;
		}

		return true;
	}

	void VulkanRenderer::DestroyShader(Shader* shader)
	{
		if (m_device.Get() == nullptr)
		{
			Logger::Error("Could not destroy shader, graphics device does not exist.");
			return;
		}

		PipelineLayout& pipelineLayout = reinterpret_cast<PipelineLayout&>(shader);
		auto address = std::addressof(pipelineLayout);

		m_pipelineLayouts.erase(std::remove_if(m_pipelineLayouts.begin(), m_pipelineLayouts.end(), [address](const PipelineLayout& s)
			{
				return std::addressof(s) == address;
			}), m_pipelineLayouts.end());

		pipelineLayout.Destroy(m_device);
	}

	bool VulkanRenderer::CreateSyncObjects()
	{
		VkDevice deviceImp = m_device.Get();

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_imageAvailableSemaphores.reserve(m_maxConcurrentFrames);
		m_renderFinishedSemaphores.reserve(m_maxConcurrentFrames);
		m_inFlightFences.reserve(m_maxConcurrentFrames);
		for (uint32_t i = 0; i < m_maxConcurrentFrames; ++i)
		{
			VkSemaphore imageAvailableSemaphore;
			VkSemaphore renderFinishedSemaphore;
			VkFence inFlightFence;
			if (vkCreateSemaphore(deviceImp, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
				vkCreateSemaphore(deviceImp, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
				vkCreateFence(deviceImp, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
			{
				Logger::Error("Failed to create synchronization objects for.");
				return false;
			}

			m_imageAvailableSemaphores.push_back(imageAvailableSemaphore);
			m_renderFinishedSemaphores.push_back(renderFinishedSemaphore);
			m_inFlightFences.push_back(inFlightFence);
		}

		return true;
	}

	bool VulkanRenderer::Initialise()
	{
		Logger::Verbose("Initialising Vulkan renderer...");

		std::string title = m_window.GetTitle();
		m_commandBuffers.resize(m_maxConcurrentFrames);

		if (!m_instance.CreateInstance(title, m_Debug, m_debug)
			|| !m_surface.CreateSurface(m_instance, m_window)
			|| !m_physicalDevice.PickPhysicalDevice(m_instance, m_surface)
			|| !m_device.CreateLogicalDevice(m_physicalDevice)
			|| !m_swapChain.CreateSwapChain(m_physicalDevice, m_device, m_surface, m_window)
			|| !m_renderPass.CreateRenderPass(m_device, m_swapChain)
			|| !m_swapChain.CreateFramebuffers(m_device, m_renderPass)
			|| !m_commandPool.CreateCommandPool(m_physicalDevice, m_device)
			|| !m_commandPool.CreateCommandBuffers(m_device, m_commandBuffers.data(), m_maxConcurrentFrames)
			|| !CreateSyncObjects())
		{
			return false;
		}

		m_running = true;
		//m_renderThread = std::thread([this]() { Render(); });

		return true;
	}

	bool VulkanRenderer::RecreateSwapChain()
	{
		vkDeviceWaitIdle(m_device.Get());

		m_resized = false;

		return m_swapChain.CreateSwapChain(m_physicalDevice, m_device, m_surface, m_window) 
			&& m_swapChain.CreateFramebuffers(m_device, m_renderPass);
	}

	void VulkanRenderer::Render()
	{
		// Skip rendering in minimised state.
		if (m_window.GetWidth() == 0 || m_window.GetHeight() == 0)
		{
			return;
		}

		VkDevice deviceImp = m_device.Get();
		VkSwapchainKHR swapchainImp = m_swapChain.Get();
		VkQueue graphicsQueue = m_device.GetGraphicsQueue();
		VkQueue presentQueue = m_device.GetPresentQueue();

		if (m_resized)
		{
			RecreateSwapChain();
			return;
		}

		//while (m_running)
		{
			if (vkWaitForFences(deviceImp, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS)
			{
				Logger::Error("Failed to wait for fences.");
				return; // abort render loop on error.
			}

			uint32_t imageIndex;
			VkResult acquireNextImageResult = vkAcquireNextImageKHR(deviceImp, swapchainImp, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
			if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR)
			{
				RecreateSwapChain();
				return; // Restart rendering.
			}
			else if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR)
			{
				Logger::Error("Failed to reset command buffer.");
				return; // abort render loop on error.
			}

			if (vkResetFences(deviceImp, 1, &m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
			{
				Logger::Error("Failed to reset fences.");
				return; // abort render loop on error.
			}

			if (vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0) != VK_SUCCESS)
			{
				Logger::Error("Failed to reset command buffer.");
				return; // abort render loop on error.
			}

			if (!RecordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex))
			{
				return; // abort render loop on error.
			}

			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

			if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
			{
				Logger::Error("Failed to submit draw command buffer.");
				return; // abort render loop on error.
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &swapchainImp;
			presentInfo.pImageIndices = &imageIndex;

			VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

			if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) 
			{
				RecreateSwapChain();
			}
			else if (presentResult != VK_SUCCESS)
			{
				Logger::Error("Failed to queue present.");
				return; // Restart rendering, don't increment current frame index.
			}
		}

		m_currentFrame = (m_currentFrame + 1) % m_maxConcurrentFrames;
	}
}