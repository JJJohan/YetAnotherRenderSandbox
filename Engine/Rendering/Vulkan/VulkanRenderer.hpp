#pragma once

#include "../Renderer.hpp"
#include <vulkan/vulkan.hpp>
#include <functional>
#include <queue>
#include "VulkanRenderStats.hpp"
#include "SwapChain.hpp"
#include "Core/Logging/Logger.hpp"
#include "UI/Vulkan/VulkanUIManager.hpp"

namespace Engine::Rendering
{
	class Shader;
	struct FrameInfoUniformBuffer;
	struct LightUniformBuffer;
	class IBuffer;
	class IRenderImage;
	class IImageView;
	class IDevice;
	class IPhysicalDevice;
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
	class CommandPool;

	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(const Engine::OS::Window& window, bool debug);
		virtual ~VulkanRenderer() override;

		virtual bool Initialise() override;
		virtual bool Render() override;

		inline virtual void SetMultiSampleCount(uint32_t multiSampleCount) override
		{
			Engine::Logging::Logger::Warning("Multisampling not supported in Vulkan backend.");
		}

		virtual void SetTemporalAAState(bool enabled) override;

		virtual void SetDebugMode(uint32_t mode);
		virtual void SetHDRState(bool enable) override;

		inline VmaAllocator GetAllocator() const { return m_allocator; };

		virtual bool SubmitResourceCommand(std::function<bool(const IDevice& device, const IPhysicalDevice& physicalDevice,
			const ICommandBuffer&, std::vector<std::unique_ptr<IBuffer>>&)> command,
			std::optional<std::function<void()>> postAction = std::nullopt) override;

	protected:
		virtual void DestroyResources() override;

	private:
		bool RecordRenderCommandBuffer(const ICommandBuffer& commandBuffer);
		bool RecordShadowCommandBuffer(const ICommandBuffer& commandBuffer);
		bool RecordPresentCommandBuffer(const ICommandBuffer& commandBuffer);
		bool RecordPostProcessingCommandBuffer(const ICommandBuffer& commandBuffer, IRenderImage& image, const IImageView& imageView);
		bool RecordUICommandBuffer(const ICommandBuffer& commandBuffer, IRenderImage& image, const IImageView& imageView);
		bool CreateSyncObjects();
		bool CreateAllocator();
		bool RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines);

		struct ResourceCommandData
		{
			std::vector<std::unique_ptr<IBuffer>> buffers;
			vk::UniqueCommandBuffer commandBuffer;
			std::optional<std::function<void()>> postAction;
		};

		std::unique_ptr<Debug> m_Debug;
		std::unique_ptr<Instance> m_instance;
		std::unique_ptr<Surface> m_surface;
		VmaAllocator m_allocator;

		std::unique_ptr<CommandPool> m_resourceCommandPool;
		std::unique_ptr<CommandPool> m_renderCommandPool;

		std::vector<vk::UniqueCommandBuffer> m_renderCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_shadowCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_combineCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_postProcessingCommandBuffers;
		std::vector<vk::UniqueCommandBuffer> m_uiCommandBuffers;
		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		vk::UniqueSemaphore m_timelineSemaphore;
		std::vector<vk::UniqueFence> m_inFlightFences;
		std::vector<std::pair<vk::UniqueFence, std::vector<ResourceCommandData>>> m_inFlightResources;
		std::vector<ResourceCommandData> m_pendingResources;

		std::queue<std::function<bool()>> m_actionQueue;

		uint64_t m_timelineValue;
		bool m_swapChainOutOfDate;
		std::mutex m_resourceSubmitMutex;
	};
}