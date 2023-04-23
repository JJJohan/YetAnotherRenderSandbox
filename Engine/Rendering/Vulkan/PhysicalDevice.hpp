#pragma once

#include <vulkan/vulkan.hpp>
#include "QueueFamilyIndices.hpp"
#include "SwapChainSupportDetails.hpp"

namespace Engine::Rendering::Vulkan
{
	class Instance;
	class Surface;

	class PhysicalDevice
	{
	public:
		PhysicalDevice();
		QueueFamilyIndices GetQueueFamilyIndices() const;
		bool PickPhysicalDevice(const Instance& instance, const Surface& surface);
		VkPhysicalDevice Get() const;
		std::vector<const char*> GetRequiredExtensions() const;

	private:

		VkPhysicalDevice m_physicalDevice;
		QueueFamilyIndices m_queueFamilyIndices;
	};
}