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
		const vk::SurfaceKHR& Get() const;
		bool Initialise(const Instance& instance, const Engine::OS::Window& window);

	private:
		vk::UniqueSurfaceKHR m_surface;
	};
}