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
		const vk::CommandPool& Get() const;
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, vk::CommandPoolCreateFlagBits flags);

		std::vector<vk::UniqueCommandBuffer> CreateCommandBuffers(const Device& device, uint32_t bufferCount);

	private:
		vk::UniqueCommandPool m_commandPool;
	};
}