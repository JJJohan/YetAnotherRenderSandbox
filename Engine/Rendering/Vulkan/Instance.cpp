#include "Instance.hpp"
#include "Debug.hpp"
#include "Core/Logging/Logger.hpp"

#if defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
		: m_instance(nullptr)
	{
	}

	VkInstance Instance::Get() const
	{
		return m_instance;
	}

	void Instance::Shutdown()
	{
		if (m_instance != nullptr)
		{
			vkDestroyInstance(m_instance, nullptr);
			m_instance = nullptr;
		}
	}

	bool CheckRequiredExtensionsSupport(const std::vector<const char*>& requestedExtensions)
	{
		uint32_t extensionCount = 0;
		VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, NULL);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance extension properties. Error code: {}", static_cast<int>(res));
			return false;
		}

		std::vector<VkExtensionProperties> properties(extensionCount);
		res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, &properties.front());
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


	bool Instance::CreateInstance(std::string name, Debug& debug, bool useDebug)
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = name.c_str();
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;

		std::vector<const char*> extensionNames;
		extensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef _WIN32    
		extensionNames.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else    
		extensionNames.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif  

		const std::vector<const char*> validationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (useDebug)
		{
			extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
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

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
		if (result != VK_SUCCESS)
		{
			Logger::Error("Failed to create Vulkan instance. Error code: {}", static_cast<int>(result));
			return false;
		}

		if (useDebug)
		{
			debug.SetupDebugCallback(*this);
		}

		return true;
	}
}