#pragma once

#include "Core/Macros.hpp"
#include "Drawer.hpp"
#include <functional>
#include <vector>

namespace Engine::Rendering
{
	class Renderer;
	class ICommandBuffer;
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

		virtual void Draw(const Engine::Rendering::ICommandBuffer& commandBuffer, float width, float height) = 0;
		inline size_t GetDrawCallbackCount() const { return m_drawCallbacks.size(); }

	protected:
		UIManager(Engine::OS::Window& window, Engine::Rendering::Renderer& renderer);
		bool Initialise();
		bool Draw(float width, float height) const;

		Engine::OS::Window& m_window;
		Engine::Rendering::Renderer& m_renderer;
		Drawer m_drawer;
		std::vector<std::function<void(const Drawer&)>> m_drawCallbacks;

	private:
		void OnDPIChanged(uint32_t dpi);

		bool m_initialised;
	};
}