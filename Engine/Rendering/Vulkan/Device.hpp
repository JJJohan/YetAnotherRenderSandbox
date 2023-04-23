#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class PhysicalDevice;

	class Device
	{
	public:
		Device();
		void Shutdown();
		VkQueue GetGraphicsQueue() const;
		VkQueue GetPresentQueue() const;
		VkDevice Get() const;
		bool CreateLogicalDevice(const PhysicalDevice& physicalDevice);

	private:
		VkDevice m_device;
		VkQueue m_graphicsQueue;
		VkQueue m_presentQueue;
	};
}