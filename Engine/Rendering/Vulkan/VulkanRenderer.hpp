#pragma once

#include "../Renderer.hpp"
#include "../Shader.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Instance.hpp"
#include "Surface.hpp"
#include "PipelineLayout.hpp"
#include "SwapChain.hpp"
#include "RenderPass.hpp"
#include "Framebuffer.hpp"
#include "CommandPool.hpp"
#include <vulkan/vulkan.hpp>
#include <thread>

namespace Engine::Rendering::Vulkan
{
	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer();

		virtual bool Initialise();
		virtual void Shutdown();
		virtual void Render();

		virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		virtual void DestroyShader(Shader* shader);

	private:
		bool RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex);
		bool CreateSyncObjects();
		bool CreateCommandBuffers();
		bool RecreateSwapChain();

		Debug m_Debug;
		Device m_device;
		PhysicalDevice m_physicalDevice;
		Instance m_instance;
		Surface m_surface;
		SwapChain m_swapChain;
		RenderPass m_renderPass;
		CommandPool m_commandPool;

		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;
		std::vector<VkFence> m_inFlightFences;

		std::vector<PipelineLayout> m_pipelineLayouts;
		std::thread m_renderThread;
		bool m_running;
		uint32_t m_currentFrame;
		const uint32_t m_maxConcurrentFrames;
	};
}