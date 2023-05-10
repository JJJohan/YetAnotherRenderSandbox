#pragma once

#include "Rendering/Vulkan/DescriptorPool.hpp"
#include "UI/UIManager.hpp"

namespace vk
{
	class Instance;
	class PhysicalDevice;
	class Queue;
	class RenderPass;
	class CommandBuffer;
}

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
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
		virtual ~VulkanUIManager();

		bool Initialise(const vk::Instance& instance, const Engine::Rendering::Vulkan::Device& device,
			const Engine::Rendering::Vulkan::PhysicalDevice& physicalDevice, const vk::RenderPass& renderPass,
			vk::CommandBuffer& setupCommandBuffer);
		void PostInitialise() const;

		void Draw(const vk::CommandBuffer& commandBuffer, float width, float height);

		bool Rebuild(const vk::Device& device, const vk::RenderPass& renderPass, vk::SampleCountFlagBits multiSampleCount) const;

	private:
		std::unique_ptr<Engine::Rendering::Vulkan::DescriptorPool> m_descriptorPool;
	};
}