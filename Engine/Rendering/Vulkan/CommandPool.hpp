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

		inline const vk::CommandPool& Get() const { return m_commandPool.get(); }
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device, vk::CommandPoolCreateFlagBits flags);

		std::vector<vk::UniqueCommandBuffer> CreateCommandBuffers(const Device& device, uint32_t bufferCount);
		vk::UniqueCommandBuffer BeginResourceCommandBuffer(const Device& device) const;

	private:
		vk::UniqueCommandPool m_commandPool;
	};
}