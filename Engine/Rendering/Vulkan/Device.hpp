#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class PhysicalDevice;

	class Device
	{
	public:
		Device();
		const vk::Queue& GetGraphicsQueue() const;
		const vk::Queue& GetPresentQueue() const;
		const vk::Device& Get() const;
		bool Initialise(const PhysicalDevice& physicalDevice);

	private:
		vk::UniqueDevice m_device;
		vk::Queue m_graphicsQueue;
		vk::Queue m_presentQueue;
	};
}