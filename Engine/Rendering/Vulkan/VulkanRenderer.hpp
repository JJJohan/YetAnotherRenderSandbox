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
#include "VulkanMeshManager.hpp"
#include <vulkan/vulkan.hpp>
#include <thread>
#include <unordered_map>
#include <concurrent_queue.h>

namespace Engine::Rendering
{
	class MeshManager;
}

namespace Engine::Rendering::Vulkan
{
	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer();

		virtual bool Initialise();
		virtual void Destroy();
		virtual void Render();

		virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		virtual void DestroyShader(Shader* shader);

		virtual MeshManager* GetMeshManager() const;

	private:
		bool RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
		bool CreateSyncObjects();
		bool CreateCommandBuffers();
		bool RecreateSwapChain(const glm::uvec2& size);

		std::unique_ptr<Debug> m_Debug;
		std::unique_ptr<Device> m_device;
		std::unique_ptr<PhysicalDevice> m_physicalDevice;
		std::unique_ptr<Instance> m_instance;
		std::unique_ptr<Surface> m_surface;
		std::unique_ptr<SwapChain> m_swapChain;
		std::unique_ptr<RenderPass> m_renderPass;

		std::unique_ptr<CommandPool> m_resourceCommandPool;
		std::unique_ptr<CommandPool> m_renderCommandPool;

		std::vector<vk::UniqueCommandBuffer> m_renderCommandBuffers;
		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
		std::vector<vk::UniqueFence> m_inFlightFences;

		std::unique_ptr<VulkanMeshManager> m_meshManager;
		std::vector<std::unique_ptr<PipelineLayout>> m_pipelineLayouts;
		concurrency::concurrent_queue<std::function<void()>> m_actionQueue;

		std::thread m_renderThread;
		bool m_running;
		bool m_swapChainOutOfDate;
		uint32_t m_currentFrame;
		const uint32_t m_maxConcurrentFrames;
	};
}