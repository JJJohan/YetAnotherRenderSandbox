#pragma once

#include "../Renderer.hpp"
#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <functional>
#include <queue>
#include <vma/vk_mem_alloc.h>

namespace Engine::Rendering
{
	class Shader;
	class SceneManager;
}

namespace Engine::UI
{
	class UIManager;
}

namespace Engine::UI::Vulkan
{
	class VulkanUIManager;
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
	class CommandPool;
	class VulkanSceneManager;
	class Buffer;
	class GBuffer;
	class ShadowMap;
	class VulkanRenderStats;

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer() override;

		virtual bool Initialise() override;
		virtual bool Render() override;
		virtual const std::vector<RenderStatsData>& GetRenderStats() const override;

		virtual void SetMultiSampleCount(uint32_t sampleCount) override;
		virtual void SetHDRState(bool enable) override;
		virtual bool IsHDRSupported() const override;

		virtual SceneManager* GetSceneManager() const override;
		virtual Engine::UI::UIManager* GetUIManager() const override;

		const Device& GetDevice() const;
		const PhysicalDevice& GetPhysicalDevice() const;
		const SwapChain& GetSwapChain() const;
		const GBuffer& GetGBuffer() const;
		VmaAllocator GetAllocator() const;
		uint32_t GetConcurrentFrameCount() const;
		const std::vector<std::unique_ptr<Buffer>>& GetFrameInfoBuffers() const;
		const std::vector<std::unique_ptr<Buffer>>& GetLightBuffers() const;

		bool SubmitResourceCommand(std::function<bool(const vk::CommandBuffer&, std::vector<std::unique_ptr<Buffer>>&)> command, std::optional<std::function<void()>> postAction = std::nullopt);

	private:
		bool RecordRenderCommandBuffer(const vk::CommandBuffer& commandBuffer);
		bool RecordShadowCommandBuffer(const vk::CommandBuffer& commandBuffer);
		bool RecordPresentCommandBuffer(const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
		bool CreateSyncObjects();
		bool CreateAllocator();
		bool RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines);
		bool CreateFrameInfoUniformBuffer();
		bool CreateLightUniformBuffer();

		void OnResize(const glm::uvec2& size);

		struct ResourceCommandData
		{
			vk::UniqueFence fence;
			std::vector<std::unique_ptr<Buffer>> buffers;
			vk::UniqueCommandBuffer commandBuffer;
			std::optional<std::function<void()>> postAction;
		};

		std::unique_ptr<Debug> m_Debug;
		std::unique_ptr<Device> m_device;
		std::unique_ptr<PhysicalDevice> m_physicalDevice;
		std::unique_ptr<Instance> m_instance;
		std::unique_ptr<Surface> m_surface;
		std::unique_ptr<SwapChain> m_swapChain;
		std::unique_ptr<GBuffer> m_gBuffer;
		std::unique_ptr<ShadowMap> m_shadowMap;
		std::unique_ptr<VulkanRenderStats> m_renderStats;
		std::unique_ptr<Engine::UI::Vulkan::VulkanUIManager> m_uiManager;
		VmaAllocator m_allocator;

		std::unique_ptr<CommandPool> m_resourceCommandPool;
		std::unique_ptr<CommandPool> m_renderCommandPool;

		std::vector<std::unique_ptr<Buffer>> m_frameInfoBuffers;
		std::vector<std::unique_ptr<Buffer>> m_lightBuffers;
		std::vector<void*> m_frameInfoBufferData;
		std::vector<void*> m_lightBufferData;

		std::vector<vk::UniqueCommandBuffer> m_renderCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_shadowCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_presentCommandBuffers;
		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
		std::vector<vk::UniqueSemaphore> m_shadowFinishedSemaphores;
		std::vector<vk::UniqueSemaphore> m_presentFinishedSemaphores;
		std::vector<vk::UniqueFence> m_inFlightFences;
		std::vector<ResourceCommandData> m_inFlightResources;

		std::unique_ptr<VulkanSceneManager> m_sceneManager;
		std::queue<std::function<bool()>> m_actionQueue;

		bool m_swapChainOutOfDate;
		uint32_t m_currentFrame;
		const uint32_t m_maxConcurrentFrames;
		glm::uvec2 m_lastWindowSize;
		std::mutex m_resourceSubmitMutex;
	};
}