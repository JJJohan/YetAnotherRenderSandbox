#pragma once

#include <vulkan/vulkan.hpp>
#include "QueueFamilyIndices.hpp"

namespace Engine::Rendering::Vulkan
{
	class Instance;
	class Surface;

	class PhysicalDevice
	{
	public:
		PhysicalDevice();
		QueueFamilyIndices GetQueueFamilyIndices() const;
		bool Initialise(const Instance& instance, const Surface& surface);
		const vk::PhysicalDevice& Get() const;
		std::vector<const char*> GetRequiredExtensions() const;
		bool FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, uint32_t* memoryType) const;
		float GetMaxAnisotropy() const;

	private:

		vk::PhysicalDevice m_physicalDevice;
		QueueFamilyIndices m_queueFamilyIndices;
		vk::PhysicalDeviceProperties m_deviceProperties;
		vk::PhysicalDeviceFeatures m_deviceFeatures;
	};
}