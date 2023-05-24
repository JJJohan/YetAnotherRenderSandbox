#pragma once

#include "Rendering/Vulkan/DescriptorPool.hpp"
#include "UI/UIManager.hpp"
#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class VulkanRenderer;
}

namespace Engine::Rendering
{
	class Renderer;
}

namespace Engine::OS
{
	class Window;
}

namespace Engine::UI::Vulkan
{
	class VulkanUIManager : public UIManager
	{
	public:
		VulkanUIManager(const Engine::OS::Window& window, Engine::Rendering::Renderer& renderer);
		virtual ~VulkanUIManager() override;

		bool Initialise(const vk::Instance& instance, Engine::Rendering::Vulkan::VulkanRenderer & renderer);

		void Draw(const vk::CommandBuffer& commandBuffer, float width, float height);

		bool Rebuild(const vk::Instance& instance, Engine::Rendering::Vulkan::VulkanRenderer& renderer);

	private:
		bool SetupRenderBackend(const vk::Instance& instance, Engine::Rendering::Vulkan::VulkanRenderer& renderer);
		bool SubmitRenderResourceSetup(Engine::Rendering::Vulkan::VulkanRenderer& renderer);

		std::unique_ptr<Engine::Rendering::Vulkan::DescriptorPool> m_descriptorPool;
		uint8_t m_initVersion;
	};
}