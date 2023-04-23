#pragma once

#include <string>
#include <memory>
#include "Core/Macros.hpp"
#include "InputState.hpp"

namespace Engine::Rendering
{
	class Renderer;
	enum class RendererType;
}

namespace Engine::OS
{
	class Window
	{
	public:
		EXPORT static std::unique_ptr<Window> Create(const std::string& title, uint32_t width, uint32_t height, bool fullscreen);
		EXPORT virtual ~Window();

		EXPORT std::string GetTitle() const;
		EXPORT uint32_t GetWidth() const;
		EXPORT uint32_t GetHeight() const;
		EXPORT bool IsFullscreen() const;
		EXPORT bool IsClosed() const;
		EXPORT virtual void* GetHandle() const;
		EXPORT virtual void* GetInstance() const;

		EXPORT virtual void SetTitle(const std::string& title);
		EXPORT virtual void SetFullscreen(bool fullscreen);
		EXPORT virtual void Resize(uint32_t width, uint32_t height);
		EXPORT virtual void Close();
		EXPORT virtual void Poll();

		void NotifyResizeEvent(uint32_t width, uint32_t height);

		EXPORT Engine::Rendering::Renderer* CreateRenderer(Engine::Rendering::RendererType rendererType, bool debug);

		InputState InputState;

	protected:
		Window(const std::string& name, uint32_t width, uint32_t height, bool fullscreen);
		std::unique_ptr<Engine::Rendering::Renderer> m_renderer;

		std::string m_title;
		uint32_t m_width;
		uint32_t m_height;
		bool m_fullscreen;
		bool m_closed;
	};
}