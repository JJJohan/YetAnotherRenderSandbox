#pragma once

#include <memory>
#include "Core/Macros.hpp"
#include "OS/Window.hpp"

namespace Engine::Rendering
{
	enum class RendererType
	{
		VULKAN
	};

	class Renderer
	{
	public:
		EXPORT static std::unique_ptr<Renderer> Create(RendererType rendererType, bool debug);
		EXPORT virtual ~Renderer();

		EXPORT virtual bool Initialise(const Engine::OS::Window& window);
		EXPORT virtual void Render();

	protected:
		Renderer(bool debug);

		bool m_debug;
	};
}