#include "Window.hpp"
#include "Core/Logging/Logger.hpp"

#ifdef _WIN32
#include "Win32/Win32Window.hpp"
#endif

using namespace Engine::Logging;

namespace Engine::OS
{
	Window::Window(const std::string& title, const glm::uvec2& size, bool fullscreen)
		: m_title(title)
		, m_size(size)
		, m_fullscreen(fullscreen)
		, m_closed(false)
		, m_resizeCallbacks()
		, m_closeCallbacks()
		, m_cursorVisible(true)
	{
	}

	Window::~Window()
	{
	}

	std::unique_ptr<Window> Window::Create(const std::string& title, const glm::uvec2& size, bool fullscreen)
	{
#ifdef _WIN32
		return Win32Window::Create(title, size, fullscreen);
#else
#error "Not implemented."
#endif
	}

	bool Window::QueryMonitorInfo(MonitorInfo& info) const
	{
		return false;
	}

	void Window::Poll()
	{
		for (const auto& callback : m_prePollCallbacks)
		{
			callback();
		}

		InputState.Update();

		for (const auto& callback : m_postPollCallbacks)
		{
			callback();
		}
	}

	void Window::Close()
	{
		m_closed = true;

		for (const auto& callback : m_closeCallbacks)
		{
			callback();
		}
	}

	void Window::OnResize(const glm::uvec2& size)
	{
		glm::uvec2 currentSize = m_size;
		if (currentSize == size)
		{
			return;
		}

		m_size = size;
		for (const auto& callback : m_resizeCallbacks)
		{
			callback(size);
		}
	}

	void Window::Resize(const glm::uvec2& size)
	{
		m_size = size;
	}
}