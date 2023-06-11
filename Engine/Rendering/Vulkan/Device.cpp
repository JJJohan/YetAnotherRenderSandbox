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

	bool Device::Initialise(const PhysicalDevice& physicalDevice)
	{
		QueueFamilyIndices indices = physicalDevice.GetQueueFamilyIndices();
		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily.value(), indices.PresentFamily.value() };
		const vk::PhysicalDeviceFeatures& availableFeatures = physicalDevice.GetFeatures();

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			vk::DeviceQueueCreateInfo queueCreateInfo;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures;
		dynamicRenderingFeatures.dynamicRendering = true;

		vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures;
		descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
		descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing = true;
		descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = true;
		descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = true;
		descriptorIndexingFeatures.runtimeDescriptorArray = true;
		descriptorIndexingFeatures.pNext = &dynamicRenderingFeatures;

		vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures;
		bufferDeviceAddressFeatures.bufferDeviceAddress = true;
		bufferDeviceAddressFeatures.pNext = &descriptorIndexingFeatures;

		vk::PhysicalDeviceVulkan11Features vulkan11Features;
		vulkan11Features.shaderDrawParameters = true;
		vulkan11Features.pNext = &bufferDeviceAddressFeatures;

		vk::PhysicalDeviceFeatures2 deviceFeatures2;
		deviceFeatures2.features.samplerAnisotropy = availableFeatures.samplerAnisotropy;
		deviceFeatures2.features.multiDrawIndirect = true;
		deviceFeatures2.features.depthClamp = availableFeatures.depthClamp;
		deviceFeatures2.features.pipelineStatisticsQuery = availableFeatures.pipelineStatisticsQuery;
		deviceFeatures2.pNext = &vulkan11Features;

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