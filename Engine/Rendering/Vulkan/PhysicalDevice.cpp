#include "PhysicalDevice.hpp"
#include "Instance.hpp"
#include "Surface.hpp"
#include "SwapChain.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Window.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	PhysicalDevice::PhysicalDevice()
		: m_physicalDevice(nullptr)
		, m_queueFamilyIndices()
		, m_deviceFeatures()
		, m_deviceProperties()
	{
	}

	const vk::PhysicalDevice& PhysicalDevice::Get() const
	{
		return m_physicalDevice;
	}

	QueueFamilyIndices PhysicalDevice::GetQueueFamilyIndices() const
	{
		return m_queueFamilyIndices;
	}

	std::vector<const char*> PhysicalDevice::GetRequiredExtensions() const
	{
		return {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			VK_EXT_HDR_METADATA_EXTENSION_NAME
		};
	}

	const vk::PhysicalDeviceLimits& PhysicalDevice::GetLimits() const
	{
		return m_deviceProperties.limits;
	}


	const vk::PhysicalDeviceFeatures& PhysicalDevice::GetFeatures() const
	{
		return m_deviceFeatures;
	}

	vk::SampleCountFlagBits PhysicalDevice::GetMaxMultiSampleCount() const
	{
		vk::SampleCountFlags counts = m_deviceProperties.limits.framebufferColorSampleCounts & m_deviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
		if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
		if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
		if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
		if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
		if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

		return vk::SampleCountFlagBits::e1;
	}

	vk::Format PhysicalDevice::FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const
	{
		for (const vk::Format& format : candidates)
		{
			vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);
			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		Logger::Error("Failed to find supported image format matching requested input.");
		return vk::Format::eUndefined;
	}

	vk::Format PhysicalDevice::FindDepthFormat() const
	{
		return FindSupportedFormat({ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
	}

	bool PhysicalDevice::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, uint32_t* memoryType) const
	{
		vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				*memoryType = i;
				return true;
			}
		}

		Logger::Error("Failed to find suitable memory type.");
		return false;
	}

	QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice& device, const Surface& surface)
	{
		QueueFamilyIndices indices{};

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		const vk::SurfaceKHR& surfaceImp = surface.Get();

		uint32_t index = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.GraphicsFamily = index;
			}

			VkBool32 presentSupport = false;
			if (device.getSurfaceSupportKHR(index, surfaceImp, &presentSupport) != vk::Result::eSuccess)
			{
				Logger::Error("Error while fetching surface support.");
				return indices;
			}

			if (presentSupport)
			{
				indices.PresentFamily = index;
			}

			if (indices.IsComplete())
			{
				break;
			}

			++index;
		}

		return indices;
	}

	struct DeviceCandidate
	{
		uint32_t Score;
		vk::PhysicalDevice Device;
		QueueFamilyIndices QueueFamilyIndices;
		SwapChainSupportDetails SwapChainSupportDetails;
		vk::PhysicalDeviceProperties Properties;
		vk::PhysicalDeviceFeatures Features;

		bool operator>(const DeviceCandidate& other) const
		{
			return Score > other.Score;
		}
	};

	bool CheckRequiredDeviceExtensionsSupport(const vk::PhysicalDevice& device, const std::vector<const char*>& requestedExtensions)
	{
		std::vector<vk::ExtensionProperties> properties = device.enumerateDeviceExtensionProperties(nullptr);
		return std::all_of(requestedExtensions.begin(), requestedExtensions.end(), [properties, requestedExtensions](const auto& extension)
			{
				return std::find_if(properties.begin(), properties.end(), [extension](const auto& prop)
					{
						return strcmp(extension, prop.extensionName) == 0;
					}) != properties.end();
			});
	}

	std::optional<DeviceCandidate> ScoreDeviceSuitability(const vk::PhysicalDevice& device, const Surface& surface, const std::vector<const char*>& extensionNames)
	{
		// Require graphics queue support.
		QueueFamilyIndices indices = FindQueueFamilies(device, surface);
		if (!indices.GraphicsFamily.has_value())
		{
			return std::nullopt;
		}

		if (!CheckRequiredDeviceExtensionsSupport(device, extensionNames))
		{
			return std::nullopt;
		}

		std::optional<SwapChainSupportDetails> swapChainSupport = SwapChain::QuerySwapChainSupport(device, surface);
		if (!swapChainSupport.has_value() || swapChainSupport->Formats.empty() || swapChainSupport->PresentModes.empty())
		{
			return std::nullopt;
		}

		vk::PhysicalDeviceProperties deviceProperties = device.getProperties();
		vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();

		uint32_t score = 0;

		// Favour dedicated GPUs
		if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			score += 1000;
		}

		// Favour higher texture size limits.
		score += deviceProperties.limits.maxImageDimension2D;

		return DeviceCandidate{ score, device, indices, swapChainSupport.value(), deviceProperties, deviceFeatures };
	}

	bool PhysicalDevice::Initialise(const Instance& instance, const Surface& surface)
	{
		const vk::Instance& instanceImp = instance.Get();

		std::vector<vk::PhysicalDevice> devices = instanceImp.enumeratePhysicalDevices();
		if (devices.empty())
		{
			Logger::Error("Failed to find GPUs with Vulkan support.");
			return false;
		}

		std::vector<const char*> extensionNames = GetRequiredExtensions();

		std::vector<DeviceCandidate> candidates;
		for (const auto& device : devices)
		{
			std::optional<DeviceCandidate> candidate = ScoreDeviceSuitability(device, surface, extensionNames);
			if (candidate.has_value())
			{
				candidates.emplace_back(candidate.value());
			}
		}

		if (candidates.empty())
		{
			Logger::Error("Failed to find GPU that met all requirements.");
			return false;
		}

		std::sort(candidates.begin(), candidates.end(), std::greater());
		const DeviceCandidate& bestCandidate = candidates.front();
		m_physicalDevice = bestCandidate.Device;
		m_queueFamilyIndices = bestCandidate.QueueFamilyIndices;
		m_deviceProperties = bestCandidate.Properties;
		m_deviceFeatures = bestCandidate.Features;

		return true;
	}
}