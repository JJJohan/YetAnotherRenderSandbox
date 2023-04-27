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

	const vk::Device& Device::Get() const
	{
		return m_device.get();
	}

	const vk::Queue& Device::GetGraphicsQueue() const
	{
		return m_graphicsQueue;
	}

	const vk::Queue& Device::GetPresentQueue() const
	{
		return m_presentQueue;
	}

	bool Device::Initialise(const PhysicalDevice& physicalDevice)
	{
		QueueFamilyIndices indices = physicalDevice.GetQueueFamilyIndices();
		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			vk::DeviceQueueCreateInfo queueCreateInfo;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
		bufferDeviceAddressFeatures.bufferDeviceAddress = true;
		vk::PhysicalDeviceFeatures2 deviceFeatures2;
		deviceFeatures2.pNext = &bufferDeviceAddressFeatures;

		std::vector<const char*> extensionNames = physicalDevice.GetRequiredExtensions();

		vk::DeviceCreateInfo createInfo;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.ppEnabledExtensionNames = extensionNames.data();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = &deviceFeatures2;

		m_device = physicalDevice.Get().createDeviceUnique(createInfo);
		if (!m_device.get())
		{
			Logger::Error("Failed to create logical device.");
			return false;
		}

		m_graphicsQueue = m_device.get().getQueue(indices.GraphicsFamily.value(), 0);
		m_presentQueue = m_device.get().getQueue(indices.PresentFamily.value(), 0);

		return true;
	}
}