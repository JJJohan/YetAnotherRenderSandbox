#pragma once

#include "../Renderer.hpp"
#include <vulkan/vulkan.hpp>
#include <unordered_map>
#include <functional>
#include <queue>
#include <vma/vk_mem_alloc.h>
#include "VulkanRenderStats.hpp"
#include "SwapChain.hpp"
#include "VulkanSceneManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "UI/Vulkan/VulkanUIManager.hpp"

namespace Engine::Rendering
{
	class Shader;
	class SceneManager;
	struct FrameInfoUniformBuffer;
	struct LightUniformBuffer;
}

namespace Engine::UI
{
	class UIManager;
}

namespace Engine::Rendering::Vulkan
{
	class Debug;
	class Device;
	class PhysicalDevice;
	class Instance;
	class Surface;
	class PipelineLayout;
	class CommandPool;
	class Buffer;
	class GBuffer;
	class ShadowMap;
	class PostProcessing;
	class RenderImage;
	class ImageView;
	class PipelineManager;

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer() override;

		virtual bool Initialise() override;
		virtual bool Render() override;
		inline virtual const std::vector<FrameStats>& GetRenderStats() const override { return m_renderStats->GetFrameStats(); };
		inline virtual const MemoryStats& GetMemoryStats() const override { return m_renderStats->GetMemoryStats(); };

		inline virtual void SetMultiSampleCount(uint32_t multiSampleCount) override
		{
			Engine::Logging::Logger::Warning("Multisampling not supported in Vulkan backend.");
		}

		virtual void SetTemporalAAState(bool enabled) override;

		virtual void SetHDRState(bool enable) override;
		inline virtual bool IsHDRSupported() const override { return m_swapChain->IsHDRCapable(); };

		inline virtual SceneManager& GetSceneManager() const override { return *m_sceneManager; };
		inline virtual Engine::UI::UIManager& GetUIManager() const override { return *m_uiManager; };

		inline const Device& GetDevice() const { return *m_device; };
		inline const PhysicalDevice& GetPhysicalDevice() const { return *m_physicalDevice; };
		inline const SwapChain& GetSwapChain() const { return *m_swapChain; };
		inline const GBuffer& GetGBuffer() const { return *m_gBuffer; };
		inline VmaAllocator GetAllocator() const { return m_allocator; };
		inline uint32_t GetConcurrentFrameCount() const { return m_maxConcurrentFrames; };
		inline const std::vector<std::unique_ptr<Buffer>>& GetFrameInfoBuffers() const { return m_frameInfoBuffers; };
		inline const std::vector<std::unique_ptr<Buffer>>& GetLightBuffers() const { return m_lightBuffers; };

		bool SubmitResourceCommand(std::function<bool(const vk::CommandBuffer&, std::vector<std::unique_ptr<Buffer>>&)> command, std::optional<std::function<void()>> postAction = std::nullopt);

	private:
		bool RecordRenderCommandBuffer(const vk::CommandBuffer& commandBuffer);
		bool RecordShadowCommandBuffer(const vk::CommandBuffer& commandBuffer);
		bool RecordPresentCommandBuffer(const vk::CommandBuffer& commandBuffer);
		bool RecordPostProcessingCommandBuffer(const vk::CommandBuffer& commandBuffer, RenderImage& image, const ImageView& imageView);
		bool RecordUICommandBuffer(const vk::CommandBuffer& commandBuffer, RenderImage& image, const ImageView& imageView);
		bool CreateSyncObjects();
		bool CreateAllocator();
		bool RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines);
		bool CreateFrameInfoUniformBuffer();
		bool CreateLightUniformBuffer();
		void UpdateFrameInfo();

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
		std::unique_ptr<PostProcessing> m_postProcessing;
		std::unique_ptr<VulkanRenderStats> m_renderStats;
		std::unique_ptr<PipelineManager> m_pipelineManager;
		std::unique_ptr<Engine::UI::Vulkan::VulkanUIManager> m_uiManager;
		VmaAllocator m_allocator;

		std::unique_ptr<CommandPool> m_resourceCommandPool;
		std::unique_ptr<CommandPool> m_renderCommandPool;

		std::vector<std::unique_ptr<Buffer>> m_frameInfoBuffers;
		std::vector<std::unique_ptr<Buffer>> m_lightBuffers;
		std::vector<FrameInfoUniformBuffer*> m_frameInfoBufferData;
		std::vector<LightUniformBuffer*> m_lightBufferData;

		std::vector<vk::UniqueCommandBuffer> m_renderCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_shadowCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_presentCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_postProcessingCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_uiCommandBuffers;
		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
		std::vector<vk::UniqueSemaphore> m_shadowFinishedSemaphores;
		std::vector<vk::UniqueSemaphore> m_presentFinishedSemaphores;
		std::vector<vk::UniqueSemaphore> m_postProcessingFinishedSemaphores;
		std::vector<vk::UniqueSemaphore> m_uiFinishedSemaphores;
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