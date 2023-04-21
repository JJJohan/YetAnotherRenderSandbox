#include "VulkanRenderer.hpp"
#include "OS/Window.hpp"
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

namespace Engine::Rendering
{
	VulkanRenderer::VulkanRenderer(bool debug)
		: Renderer(debug)
		, m_instance(nullptr)
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
	}

	bool EnumerateExtensions(std::vector<VkExtensionProperties> properties)
	{
		uint32_t extensionCount = 0;
		VkResult res = vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance extension properties. Error code: {}", static_cast<int>(res));
			return false;
		}

		properties.resize(extensionCount);

		res = vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, &properties.front());
		if (res != VK_SUCCESS)
		{
			Logger::Error("Failed to enumerate instance extension properties. Error code: {}", static_cast<int>(res));
			return false;
		}

		Logger::Verbose("Enumerated {} available extensions:", extensionCount);
		for (VkExtensionProperties property : properties)
		{
			Logger::Verbose(" - {}", property.extensionName);
		}

		return true;
	}

	bool VulkanRenderer::Initialise(const Engine::OS::Window& window)
	{
		Logger::Verbose("Initialising Vulkan renderer...");

		std::string title = window.GetTitle();

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = title.c_str();
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
		if (m_debug)
		{
			extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
		createInfo.ppEnabledExtensionNames = &extensionNames.front();

		return true;
	}

	void VulkanRenderer::Render()
	{
	}
}