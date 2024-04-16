#include "Debug.hpp"
#include "Instance.hpp"
#include "Core/Logger.hpp"

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#else
#define DEBUG_BREAK (void)
#endif

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

	void Debug::PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) const
	{
		createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
		createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
		createInfo.pfnUserCallback = debugCallback;
	}

	void Debug::SetupDebugCallback(const Instance& instance)
	{
		const vk::Instance& inst = instance.Get();

		vk::DebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		m_debugMessenger = inst.createDebugUtilsMessengerEXTUnique(createInfo);
	}
}