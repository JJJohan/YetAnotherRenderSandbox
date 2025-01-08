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
	class ICommandBuffer;
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
		VulkanUIManager(Engine::OS::Window& window, Engine::Rendering::Renderer& renderer);
		virtual ~VulkanUIManager() override;

		bool Initialise(const vk::Instance& instance, Engine::Rendering::Vulkan::VulkanRenderer& renderer);

		virtual void Draw(const Engine::Rendering::ICommandBuffer& commandBuffer, float width, float height) override;

		bool Rebuild(const vk::Instance& instance, Engine::Rendering::Vulkan::VulkanRenderer& renderer);

	private:
		bool SetupRenderBackend(const vk::Instance& instance, Engine::Rendering::Vulkan::VulkanRenderer& renderer);

		std::unique_ptr<Engine::Rendering::Vulkan::DescriptorPool> m_descriptorPool;
	};
}