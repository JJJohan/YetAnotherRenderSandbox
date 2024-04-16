#include "Instance.hpp"
#include "Debug.hpp"
#include "Core/Logging/Logger.hpp"

#if defined (_WIN32)
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#include <vulkan/vulkan_xcb.h>
#elif defined(__ANDROID__)
#include <vulkan/vulkan_android.h>
#endif

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Instance::Instance()
		: m_instance()
	{
	}

	bool CheckRequiredExtensionsSupport(const std::vector<const char*>& requestedExtensions)
	{
		std::vector<vk::ExtensionProperties> properties = vk::enumerateInstanceExtensionProperties(nullptr);
		if (properties.empty())
		{
			Logger::Error("Failed to enumerate instance extension properties.");
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

	bool Instance::Initialise(std::string name, Debug& debug, bool useDebug)
	{
		vk::ApplicationInfo appInfo(name.c_str(), VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

		vk::InstanceCreateInfo createInfo{};
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;

		std::vector<const char*> extensionNames;
		extensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extensionNames.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
#ifdef _WIN32    
		extensionNames.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else    
		extensionNames.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif  

		const std::vector<const char*> validationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};

		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (useDebug)
		{
			extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

			if (debug.CheckValidationLayerSupport(validationLayers))
			{
				createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();
			}
			else
			{
				Logger::Warning("Not all requested validation layers are available.");
			}

			debug.PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = &debugCreateInfo;
		}

		if (!CheckRequiredExtensionsSupport(extensionNames))
		{
			Logger::Error("Not all requested extensions are available.");
			return false;
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
		createInfo.ppEnabledExtensionNames = &extensionNames.front();

		m_instance = vk::createInstanceUnique(createInfo);
		if (!m_instance.get())
		{
			Logger::Error("Failed to create Vulkan instance.");
			return false;
		}

		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

		if (useDebug)
		{
			debug.SetupDebugCallback(*this);
		}

		return true;
	}
}