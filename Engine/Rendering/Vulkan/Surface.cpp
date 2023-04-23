#include "Surface.hpp"
#include "Instance.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Window.hpp"

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
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	Surface::Surface()
		: m_surface(nullptr)
	{
	}

	VkSurfaceKHR Surface::Get() const
	{
		return m_surface;
	}

	void Surface::Shutdown(const Instance& instance)
	{
		if (m_surface != nullptr)
		{
			vkDestroySurfaceKHR(instance.Get(), m_surface, nullptr);
			m_surface = nullptr;
		}
	}

	bool Surface::CreateSurface(const Instance& instance, const Window& window)
	{
#ifdef WIN32
		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = (HWND)window.GetHandle();
		createInfo.hinstance = (HINSTANCE)window.GetInstance();

		if (vkCreateWin32SurfaceKHR(instance.Get(), &createInfo, nullptr, &m_surface) != VK_SUCCESS)
		{
			Logger::Error("Failed to create window surface.");
			return false;
		}
#else
#error "Unsupported platform"
#endif

		return true;
	}
}