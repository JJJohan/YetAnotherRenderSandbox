#pragma once

#include <vulkan/vulkan.hpp>

namespace Engine::OS
{
	class Window;
}

namespace Engine::Rendering::Vulkan
{
	class Instance;

	class Surface
	{
	public:
		Surface();
		void Shutdown(const Instance& instance);
		VkSurfaceKHR Get() const;
		bool CreateSurface(const Instance& instance, const Engine::OS::Window& window);

	private:
		VkSurfaceKHR m_surface;
	};
}