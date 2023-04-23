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
	{
	}

	VkPhysicalDevice PhysicalDevice::Get() const
	{
		return m_physicalDevice;
	}

	QueueFamilyIndices PhysicalDevice::GetQueueFamilyIndices() const
	{
		return m_queueFamilyIndices;
	}

	std::vector<const char*> PhysicalDevice::GetRequiredExtensions() const
	{
		return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	}

	QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device, const Surface& surface)
	{
		QueueFamilyIndices indices{};

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		VkSurfaceKHR surfaceImp = surface.Get();

		uint32_t index = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.GraphicsFamily = index;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surfaceImp, &presentSupport);
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
		VkPhysicalDevice Device;
		QueueFamilyIndices QueueFamilyIndices;
		SwapChainSupportDetails SwapChainSupportDetails;

		bool operator>(const DeviceCandidate& other) const
		{
			return Score > other.Score;
		}
	};

	bool CheckRequiredDeviceExtensionsSupport(const VkPhysicalDevice& device, const std::vector<const char*>& requestedExtensions)
	{
		uint32_t extensionCount = 0;
		VkResult res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance extension properties. Error code: {}", static_cast<int>(res));
			return false;
		}

		std::vector<VkExtensionProperties> properties(extensionCount);
		res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, &properties.front());
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance extension properties. Error code: {}", static_cast<int>(res));
			return false;
		}

		return std::all_of(requestedExtensions.begin(), requestedExtensions.end(), [properties, requestedExtensions](const auto& extension)
			{
				return std::find_if(properties.begin(), properties.end(), [extension](const auto& prop)
					{
						return strcmp(extension, prop.extensionName) == 0;
					}) != properties.end();
			});
	}

	std::optional<DeviceCandidate> ScoreDeviceSuitability(const VkPhysicalDevice& device, const Surface& surface, const std::vector<const char*>& extensionNames)
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

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		uint32_t score = 0;

		// Favour dedicated GPUs
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		// Favour higher texture size limits.
		score += deviceProperties.limits.maxImageDimension2D;

		return DeviceCandidate { score, device, indices, swapChainSupport.value()};
	}

	bool PhysicalDevice::PickPhysicalDevice(const Instance& instance, const Surface& surface)
	{
		VkInstance instanceImp = instance.Get();

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instanceImp, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			Logger::Error("Failed to find GPUs with Vulkan support.");
			return false;
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instanceImp, &deviceCount, devices.data());
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
		if (candidates.size() == 0)
		{
			Logger::Error("Failed to find GPU that met all requirements.");
			return false;
		}

		std::sort(candidates.begin(), candidates.end(), std::greater());
		const DeviceCandidate& bestCandidate = candidates.front();
		m_physicalDevice = bestCandidate.Device;
		m_queueFamilyIndices = bestCandidate.QueueFamilyIndices;

		return true;
	}
}