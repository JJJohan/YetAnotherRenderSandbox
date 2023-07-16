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
	class ISemaphore;
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

		virtual void SetHDRState(bool enable) override;

		inline VmaAllocator GetAllocator() const { return m_allocator; };

		virtual bool SubmitResourceCommand(std::function<bool(const IDevice& device, const IPhysicalDevice& physicalDevice,
			const ICommandBuffer&, std::vector<std::unique_ptr<IBuffer>>&)> command,
			std::optional<std::function<void()>> postAction = std::nullopt) override;

		virtual IRenderImage& GetPresentImage() const override;
		virtual bool Present(const std::vector<SubmitInfo>& submitInfos) override;

	protected:
		virtual void DestroyResources() override;

	private:
		bool CreateSyncObjects();
		bool CreateAllocator();
		bool RecreateSwapChain(const glm::uvec2& size, bool rebuildPipelines);

		struct ResourceCommandData
		{
			std::vector<std::unique_ptr<IBuffer>> buffers;
			std::unique_ptr<ICommandBuffer> commandBuffer;
			std::optional<std::function<void()>> postAction;
		};

		std::unique_ptr<Debug> m_Debug;
		std::unique_ptr<Instance> m_instance;
		std::unique_ptr<Surface> m_surface;
		VmaAllocator m_allocator;
		uint32_t m_presentImageIndex;

		std::unique_ptr<CommandPool> m_resourceCommandPool;

		std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
		std::vector<vk::UniqueFence> m_inFlightFences;
		std::vector<std::pair<vk::UniqueFence, std::vector<ResourceCommandData>>> m_inFlightResources;
		std::vector<ResourceCommandData> m_pendingResources;

		std::queue<std::function<bool()>> m_actionQueue;

		bool m_swapChainOutOfDate;
		std::mutex m_resourceSubmitMutex;
	};
}