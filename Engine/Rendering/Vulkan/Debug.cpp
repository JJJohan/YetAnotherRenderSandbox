#include "Debug.hpp"
#include "Instance.hpp"
#include "Core/Logging/Logger.hpp"

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK (void)
#endif

using namespace Engine::Logging;

namespace Engine::Rendering::Vulkan
{
	Debug::Debug()
		: m_debugMessenger(nullptr)
	{
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		switch (messageSeverity)
		{
		default:
			Logger::Warning("Unexpected Vulkan severity enum, treating it as a debug message.");
			[[fallthrough]];

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			Logger::Warning("Vulkan - {}", pCallbackData->pMessage);
			break;

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			Logger::Error("Vulkan - {}", pCallbackData->pMessage);
			DEBUG_BREAK;
			break;

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			Logger::Verbose("Vulkan - {}", pCallbackData->pMessage);
			break;

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			Logger::Info("Vulkan - {}", pCallbackData->pMessage);
			break;
		}

		return VK_FALSE;
	}

	VkResult CreateDebugUtilsMessengerEXT(const Instance& instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		VkInstance inst = instance.Get();

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkCreateDebugUtilsMessengerEXT");
		if (func == nullptr)
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}

		return func(inst, pCreateInfo, pAllocator, pDebugMessenger);
	}

	VkResult DestroyDebugUtilsMessengerEXT(const Instance& instance, const VkDebugUtilsMessengerEXT& debugMessenger,
		const VkAllocationCallbacks* pAllocator)
	{
		VkInstance inst = instance.Get();

		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
		if (func == nullptr)
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}

		func(inst, debugMessenger, pAllocator);
		return VK_SUCCESS;
	}

	bool Debug::CheckValidationLayerSupport(const std::vector<const char*>& validationLayers) const
	{
		uint32_t layerCount;
		VkResult res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance layers. Error code: {}", static_cast<int>(res));
			return false;
		}

		std::vector<VkLayerProperties> availableLayers(layerCount);
		res = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance layers. Error code: {}", static_cast<int>(res));
			return false;
		}

		return std::all_of(validationLayers.begin(), validationLayers.end(), [availableLayers, validationLayers](const auto& layerName)
			{
				return std::find_if(availableLayers.begin(), availableLayers.end(), [layerName](const auto& layer)
					{
						return strcmp(layerName, layer.layerName) == 0;
					}) != availableLayers.end();
			});
	}

	void Debug::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	bool Debug::SetupDebugCallback(const Instance& instance)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		PopulateDebugMessengerCreateInfo(createInfo);

		return CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &m_debugMessenger) == VK_SUCCESS;
	}

	bool Debug::RemoveDebugCallback(const Instance& instance) const
	{
		return DestroyDebugUtilsMessengerEXT(instance, m_debugMessenger, nullptr) == VK_SUCCESS;
	}
}