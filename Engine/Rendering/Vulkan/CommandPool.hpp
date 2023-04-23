#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class PhysicalDevice;
	class Device;

	class CommandPool
	{
	public:
		CommandPool();
		void Shutdown(const Device& device);
		bool CreateCommandPool(const PhysicalDevice& physicalDevice, const Device& device);

		bool CreateCommandBuffers(const Device& device, VkCommandBuffer* commandBuffers, uint32_t bufferCount);

	private:
		VkCommandPool m_commandPool;
	};
}