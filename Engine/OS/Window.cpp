#include "Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "Rendering/Renderer.hpp"

#ifdef _WIN32
#include "Win32/Win32Window.hpp"
#endif

using namespace Engine::Logging;
using namespace Engine::Rendering;

namespace Engine::OS
{
	Window::Window(const std::string& title, uint32_t width, uint32_t height, bool fullscreen)
		: m_title(title)
		, m_width(width)
		, m_height(height)
		, m_fullscreen(fullscreen)
		, m_closed(false)
		, m_renderer()
	{
	}

	Window::~Window()
	{
		Close();
	}

	std::unique_ptr<Window> Window::Create(const std::string& title, uint32_t width, uint32_t height, bool fullscreen)
	{
#ifdef _WIN32
		return Win32Window::Create(title, width, height, fullscreen);
#else
#error "Not implemented."
#endif
	}

	void Window::Poll()
	{
		InputState.Update();
	}

	void Window::Close()
	{
		m_renderer.reset();
	}

	std::string Window::GetTitle() const
	{
		return m_title;
	}

	uint32_t Window::GetWidth() const
	{
		return m_width;
	}

	uint32_t Window::GetHeight() const
	{
		return m_height;
	}

	bool Window::IsFullscreen() const
	{
		return m_fullscreen;
	}

	bool Window::IsClosed() const
	{
		return m_closed;
	}

	void* Window::GetHandle() const
	{
		return nullptr;
	}

	void* Window::GetInstance() const
	{
		return nullptr;
	}

	void Window::SetTitle(const std::string& title)
	{
		m_title = title;
	}

	void Window::SetFullscreen(bool fullscreen)
	{
		m_fullscreen = fullscreen;
	}

	void Window::NotifyResizeEvent(uint32_t width, uint32_t height)
	{
		if (m_width == width && m_height == height)
		{
			return;
		}

		m_width = width;
		m_height = height;

		Renderer* renderer = m_renderer.get();
		if (renderer)
		{
			renderer->NotifyResizeEvent();
		}
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
	}

	Renderer* Window::CreateRenderer(RendererType rendererType, bool debug)
	{
		if (m_renderer.get() != nullptr)
		{
			Logger::Warning("Renderer already created, returning it instead.");
			return m_renderer.get();
		}

		m_renderer = Renderer::Create(rendererType, *this, debug);
		return m_renderer.get();
	}
}