#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering
{
	class IDevice;
	class IPhysicalDevice;
}

namespace Engine::Rendering::Vulkan
{
	class CommandPool
	{
	public:
		CommandPool();

		inline const vk::CommandPool& Get() const { return m_commandPool.get(); }
		bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t queueFamilyIndex, vk::CommandPoolCreateFlagBits flags);

		std::vector<vk::UniqueCommandBuffer> CreateCommandBuffers(const IDevice& device, uint32_t bufferCount);
		vk::UniqueCommandBuffer BeginResourceCommandBuffer(const IDevice& device) const;

	private:
		vk::UniqueCommandPool m_commandPool;
	};
}