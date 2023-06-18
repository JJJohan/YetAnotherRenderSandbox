#pragma once

#include "Core/Macros.hpp"
#include "Drawer.hpp"
#include <functional>
#include <vector>

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
	class Drawer;

	class UIManager
	{
	public:
		virtual ~UIManager();

		EXPORT void RegisterDrawCallback(std::function<void(const Drawer&)> callback);
		EXPORT void UnregisterDrawCallback(std::function<void(const Drawer&)> callback);
		EXPORT float GetFPS() const;

	protected:
		UIManager(const Engine::OS::Window& window, Engine::Rendering::Renderer& renderer);
		bool Initialise() const;
		bool Draw(float width, float height) const;

		const Engine::OS::Window& m_window;
		Engine::Rendering::Renderer& m_renderer;
		Drawer m_drawer;
		std::vector<std::function<void(const Drawer&)>> m_drawCallbacks;
	};
}