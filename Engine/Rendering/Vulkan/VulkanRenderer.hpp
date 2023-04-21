#pragma once

#include "../Renderer.hpp"
#include <vulkan/vulkan.hpp>

namespace Engine::Rendering
{
	class VulkanRenderer : public Renderer
	{
	public:
		VulkanRenderer(bool debug);
		virtual ~VulkanRenderer();

		virtual bool Initialise(const Engine::OS::Window& window);
		virtual void Render();

	private:
		VkInstance m_instance;
	};
}