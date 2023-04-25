#include "Surface.hpp"
#include "Instance.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Window.hpp"

using namespace Engine::Logging;
using namespace Engine::OS;

namespace Engine::Rendering::Vulkan
{
	Surface::Surface()
		: m_surface(nullptr)
	{
	}

	const vk::SurfaceKHR& Surface::Get() const
	{
		return m_surface.get();
	}

	bool Surface::Initialise(const Instance& instance, const Window& window)
	{
#ifdef WIN32
		vk::Win32SurfaceCreateInfoKHR createInfo;
		createInfo.hwnd = (HWND)window.GetHandle();
		createInfo.hinstance = (HINSTANCE)window.GetInstance();

		m_surface = instance.Get().createWin32SurfaceKHRUnique(createInfo);
		if (!m_surface.get())
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