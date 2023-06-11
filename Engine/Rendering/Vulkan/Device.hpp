#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class PhysicalDevice;

	class Device
	{
	public:
		Device();
		inline const vk::Device& Get() const { return m_device.get(); }
		inline const vk::Queue& GetGraphicsQueue() const { return m_graphicsQueue; }
		inline const vk::Queue& GetPresentQueue() const { return m_presentQueue; }
		bool Initialise(const PhysicalDevice& physicalDevice);

	private:
		vk::UniqueDevice m_device;
		vk::Queue m_graphicsQueue;
		vk::Queue m_presentQueue;
	};
}