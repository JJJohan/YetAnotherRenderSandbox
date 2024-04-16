#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "Core/Logger.hpp"
#include <set>

namespace Engine::Rendering::Vulkan
{
	Device::Device()
		: m_device(nullptr)
		, m_graphicsQueue(nullptr)
		, m_presentQueue(nullptr)
		, m_computeQueue(nullptr)
	{
	}

	bool Device::Initialise(const IPhysicalDevice& physicalDevice)
	{
		const PhysicalDevice& vkPhysicalDevice = static_cast<const PhysicalDevice&>(physicalDevice);

		QueueFamilyIndices indices = vkPhysicalDevice.GetQueueFamilyIndices();
		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {
			indices.GraphicsFamily.value(), indices.TransferFamily.value(), indices.PresentFamily.value(), indices.ComputeFamily.value() };

		const vk::PhysicalDeviceFeatures& availableFeatures = vkPhysicalDevice.GetFeatures();

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			vk::DeviceQueueCreateInfo queueCreateInfo;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		const std::vector<const char*>& optionalExtensionNames = vkPhysicalDevice.GetSupportedOptionalExtensions();

		// Chain optional extensions.
		void* nextExtension = nullptr;
		for (const char* extension : optionalExtensionNames)
		{
			// None of the current optional extensions require additional device features.
		}

		vk::PhysicalDeviceVulkan13Features vulkan13Features;
		vulkan13Features.dynamicRendering = true;
		vulkan13Features.synchronization2 = true;
		vulkan13Features.pNext = nextExtension;

		vk::PhysicalDeviceVulkan12Features vulkan12Features;
		vulkan12Features.bufferDeviceAddress = true;
		vulkan12Features.timelineSemaphore = true;
		vulkan12Features.shaderSampledImageArrayNonUniformIndexing = true;
		vulkan12Features.shaderUniformBufferArrayNonUniformIndexing = true;
		vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = true;
		vulkan12Features.descriptorBindingVariableDescriptorCount = true;
		vulkan12Features.runtimeDescriptorArray = true;
		vulkan12Features.shaderOutputViewportIndex = true;
		vulkan12Features.shaderOutputLayer = true;
		vulkan12Features.hostQueryReset = true;
		vulkan12Features.samplerFilterMinmax = true;
		vulkan12Features.drawIndirectCount = true;
		vulkan12Features.pNext = &vulkan13Features;

		vk::PhysicalDeviceVulkan11Features vulkan11Features;
		vulkan11Features.shaderDrawParameters = true;
		vulkan11Features.pNext = &vulkan12Features;

		vk::PhysicalDeviceFeatures2 deviceFeatures2;
		deviceFeatures2.features.samplerAnisotropy = availableFeatures.samplerAnisotropy;
		deviceFeatures2.features.multiDrawIndirect = true;
		deviceFeatures2.features.depthClamp = availableFeatures.depthClamp;
		deviceFeatures2.features.pipelineStatisticsQuery = availableFeatures.pipelineStatisticsQuery;
		deviceFeatures2.pNext = &vulkan11Features;

		std::vector<const char*> extensionNames = vkPhysicalDevice.GetRequiredExtensions();
		extensionNames.append_range(optionalExtensionNames);

		vk::DeviceCreateInfo createInfo;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.ppEnabledExtensionNames = extensionNames.data();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = &deviceFeatures2;

		m_device = vkPhysicalDevice.Get().createDeviceUnique(createInfo);
		if (!m_device.get())
		{
			Logger::Error("Failed to create logical device.");
			return false;
		}

		m_graphicsQueue = m_device->getQueue(indices.GraphicsFamily.value(), 0);
		m_presentQueue = m_device->getQueue(indices.PresentFamily.value(), 0);
		m_computeQueue = m_device->getQueue(indices.ComputeFamily.value(), 0);
		m_transferQueue = m_device->getQueue(indices.TransferFamily.value(), 0);

		return true;
	}
}