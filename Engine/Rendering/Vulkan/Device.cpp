#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Core/Logging/Logger.hpp"
#include <set>

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Device::Device()
		: m_device(nullptr)
		, m_graphicsQueue(nullptr)
		, m_presentQueue(nullptr)
	{
	}

	VkDevice Device::Get() const
	{
		return m_device;
	}

	VkQueue Device::GetGraphicsQueue() const
	{
		return m_graphicsQueue;
	}

	VkQueue Device::GetPresentQueue() const
	{
		return m_presentQueue;
	}

	void Device::Shutdown()
	{
		if (m_device != nullptr)
		{
			vkDestroyDevice(m_device, nullptr);
			m_device = nullptr;
		}
	}

	bool Device::CreateLogicalDevice(const PhysicalDevice& physicalDevice)
	{
		QueueFamilyIndices indices = physicalDevice.GetQueueFamilyIndices();
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		std::vector<const char*> extensionNames = physicalDevice.GetRequiredExtensions();


		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.ppEnabledExtensionNames = extensionNames.data();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
		createInfo.enabledLayerCount = 0;

		if (vkCreateDevice(physicalDevice.Get(), &createInfo, nullptr, &m_device) != VK_SUCCESS)
		{
			Logger::Error("Failed to create logical device.");
			return false;
		}

		vkGetDeviceQueue(m_device, indices.GraphicsFamily.value(), 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_device, indices.PresentFamily.value(), 0, &m_presentQueue);


		return true;
	}
}