#pragma once

#include "Core/Macros.hpp"
#include <memory>

namespace Engine::Rendering
{
	class Renderer;
}

namespace Engine::OS
{
	class Window;
}

namespace Engine::UI
{
	class UIManager
	{
	public:
		virtual ~UIManager();

	protected:
		UIManager(const Engine::OS::Window& window, Engine::Rendering::Renderer& renderer);

	protected:
		const Engine::OS::Window& m_window;
		Engine::Rendering::Renderer& m_renderer;
	};
}