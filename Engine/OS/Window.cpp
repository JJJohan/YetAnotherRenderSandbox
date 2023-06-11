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

	void Window::RegisterResizeCallback(std::function<void(const glm::uvec2&)> callback)
	{
		const auto& target = callback.target<void(const glm::uvec2&)>();
		for (auto it = m_resizeCallbacks.cbegin(); it != m_resizeCallbacks.cend(); ++it)
		{
			if (it->target<void(const glm::uvec2&)>() == target)
			{
				Logger::Error("Callback already registered.");
				return;
			}
		}

		m_resizeCallbacks.push_back(callback);
	}

	void Window::UnregisterResizeCallback(std::function<void(const glm::uvec2&)> callback)
	{
		const auto& target = callback.target<void(const glm::uvec2&)>();
		for (auto it = m_resizeCallbacks.cbegin(); it != m_resizeCallbacks.cend(); ++it)
		{
			if (it->target<void(const glm::uvec2&)>() == target)
			{
				m_resizeCallbacks.erase(it);
				return;
			}
		}

		Logger::Error("Callback was not registered.");
	}

	void Window::RegisterCloseCallback(std::function<void(void)> callback)
	{
		const auto& target = callback.target<void>();
		for (auto it = m_closeCallbacks.cbegin(); it != m_closeCallbacks.cend(); ++it)
		{
			if (it->target<void>() == target)
			{
				Logger::Error("Callback already registered.");
				return;
			}
		}

		m_closeCallbacks.push_back(callback);
	}

	void Window::UnregisterCloseCallback(std::function<void(void)> callback)
	{
		const auto& target = callback.target<void>();
		for (auto it = m_closeCallbacks.cbegin(); it != m_closeCallbacks.cend(); ++it)
		{
			if (it->target<void>() == target)
			{
				m_closeCallbacks.erase(it);
				return;
			}
		}

		Logger::Error("Callback was not registered.");
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