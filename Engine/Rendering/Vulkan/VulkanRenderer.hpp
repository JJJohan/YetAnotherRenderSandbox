#pragma once

#include "../Renderer.hpp"
#include <vulkan/vulkan.hpp>
#include <thread>
#include <unordered_map>
#include <concurrent_queue.h>
#include <functional>

struct VmaAllocator_T;

namespace Engine::Rendering
{
	class Shader;
	class SceneManager;
}

namespace Engine::Rendering::Vulkan
{
	class Debug;
	class Device;
	class PhysicalDevice;
	class Instance;
	class Surface;
	class PipelineLayout;
	class SwapChain;
	class RenderPass;
	class Framebuffer;
	class CommandPool;
	class VulkanSceneManager;
	class Buffer;

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer();

		virtual bool Initialise();
		virtual void Render();

		virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		virtual void DestroyShader(Shader* shader);

		virtual void SetMultiSampleCount(uint32_t sampleCount);

		virtual SceneManager* GetSceneManager() const;

	private:
		bool RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
		bool CreateSyncObjects();
		bool CreateAllocator();
		bool RecreateSwapChain(const glm::uvec2& size);
		bool CreateFrameInfoUniformBuffer();

		vk::SampleCountFlagBits GetMultiSampleCount(uint32_t sampleCount) const;
		uint32_t MultiSampleCountToInteger(vk::SampleCountFlagBits sampleCount) const;

		std::unique_ptr<Debug> m_Debug;
		std::unique_ptr<Device> m_device;
		std::unique_ptr<PhysicalDevice> m_physicalDevice;
		std::unique_ptr<Instance> m_instance;
		std::unique_ptr<Surface> m_surface;
		std::unique_ptr<SwapChain> m_swapChain;
		std::unique_ptr<RenderPass> m_renderPass;
		struct VmaAllocator_T* m_allocator;

		std::unique_ptr<CommandPool> m_resourceCommandPool;
		std::unique_ptr<CommandPool> m_renderCommandPool;

		std::vector<std::unique_ptr<Buffer>> m_frameInfoBuffers;
		std::vector<void*> m_frameInfoBufferData;

		std::vector<vk::UniqueCommandBuffer> m_renderCommandBuffers;
		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
		std::vector<vk::UniqueFence> m_inFlightFences;

		std::unique_ptr<VulkanSceneManager> m_sceneManager;
		std::vector<std::unique_ptr<PipelineLayout>> m_pipelineLayouts;
		concurrency::concurrent_queue<std::function<bool()>> m_actionQueue;

		std::thread m_renderThread;
		bool m_running;
		bool m_swapChainOutOfDate;
		uint32_t m_currentFrame;
		const uint32_t m_maxConcurrentFrames;
	};
}