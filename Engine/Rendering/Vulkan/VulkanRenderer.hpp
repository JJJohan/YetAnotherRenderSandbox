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

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer();

		virtual bool Initialise();
		virtual bool Render();

		virtual Shader* CreateShader(const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs);
		virtual void DestroyShader(Shader* shader);

		virtual void SetMultiSampleCount(uint32_t sampleCount) override;
		virtual void SetHDRState(bool enable) override;
		virtual bool IsHDRSupported() const override;

		virtual SceneManager* GetSceneManager() const;
		virtual Engine::UI::UIManager* GetUIManager() const;

		const Device& GetDevice() const;
		const PhysicalDevice& GetPhysicalDevice() const;
		const SwapChain& GetSwapChain() const;
		VmaAllocator GetAllocator() const;
		uint32_t GetConcurrentFrameCount() const;
		const std::vector<std::unique_ptr<Buffer>>& GetFrameInfoBuffers() const;

		bool SubmitResourceCommand(std::function<bool(const vk::CommandBuffer&,std::vector<std::unique_ptr<Buffer>>&)> command, std::optional<std::function<void()>> postAction = std::nullopt);

	private:
		bool RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
		bool CreateSyncObjects();
		bool CreateAllocator();
		bool RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines);
		bool CreateFrameInfoUniformBuffer();

		void OnResize(const glm::uvec2& size);

		vk::SampleCountFlagBits GetMultiSampleCount(uint32_t sampleCount) const;
		uint32_t MultiSampleCountToInteger(vk::SampleCountFlagBits sampleCount) const;

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
		std::unique_ptr<Engine::UI::Vulkan::VulkanUIManager> m_uiManager;
		VmaAllocator m_allocator;

		std::unique_ptr<CommandPool> m_resourceCommandPool;
		std::unique_ptr<CommandPool> m_renderCommandPool;

		std::vector<std::unique_ptr<Buffer>> m_frameInfoBuffers;
		std::vector<void*> m_frameInfoBufferData;

		std::vector<vk::UniqueCommandBuffer> m_renderCommandBuffers;
		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
		std::vector<vk::UniqueFence> m_inFlightFences;
		std::vector<ResourceCommandData> m_inFlightResources;

		std::unique_ptr<VulkanSceneManager> m_sceneManager;
		std::vector<std::unique_ptr<PipelineLayout>> m_pipelineLayouts;
		std::queue<std::function<bool()>> m_actionQueue;

		bool m_swapChainOutOfDate;
		uint32_t m_currentFrame;
		const uint32_t m_maxConcurrentFrames;
		glm::uvec2 m_lastWindowSize;
	};
}