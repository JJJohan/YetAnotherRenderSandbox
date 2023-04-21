#include "Window.hpp"
#ifdef _WIN32
#include "Win32/Win32Window.hpp"
#endif

namespace Engine::OS
{
	Window::Window(const std::string& title, uint32_t width, uint32_t height, bool fullscreen)
		: m_title(title)
		, m_width(width)
		, m_height(height)
		, m_fullscreen(fullscreen)
		, m_closed(false)
	{
	}

	Window::~Window()
	{
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

	void Window::SetTitle(const std::string& title)
	{
		m_title = title;
	}

	void Window::SetFullscreen(bool fullscreen)
	{
		m_fullscreen = fullscreen;
	}

	void Window::Resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
	}
}