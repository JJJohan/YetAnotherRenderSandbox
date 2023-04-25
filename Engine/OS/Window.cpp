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

	void Window::Poll()
	{
		InputState.Update();
	}

	void Window::Close()
	{
		m_closed = true;

		for (const auto& callback : m_closeCallbacks)
		{
			callback();
		}
	}

	const std::string& Window::GetTitle() const
	{
		return m_title;
	}

	const glm::uvec2 Window::GetSize() const
	{
		return m_size;
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

	void Window::RegisterResizeCallback(std::function<void(glm::uvec2)> callback)
	{
		m_resizeCallbacks.push_back(callback);
	}

	void Window::RegisterCloseCallback(std::function<void(void)> callback)
	{
		m_closeCallbacks.push_back(callback);
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