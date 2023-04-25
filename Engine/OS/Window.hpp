#pragma once

#include <string>
#include <memory>
#include "Core/Macros.hpp"
#include "InputState.hpp"
#include <functional>

namespace Engine::OS
{
	class Window
	{
	public:
		EXPORT static std::unique_ptr<Window> Create(const std::string& title, const glm::uvec2& size, bool fullscreen);
		EXPORT virtual ~Window();

		EXPORT const std::string& GetTitle() const;
		EXPORT const glm::uvec2 GetSize() const;
		EXPORT bool IsFullscreen() const;
		EXPORT bool IsClosed() const;
		EXPORT virtual void* GetHandle() const;
		EXPORT virtual void* GetInstance() const;

		EXPORT virtual void SetTitle(const std::string& title);
		EXPORT virtual void SetFullscreen(bool fullscreen);
		EXPORT virtual void Resize(const glm::uvec2& size);
		EXPORT virtual void Close();
		EXPORT virtual void Poll();

		EXPORT void RegisterResizeCallback(std::function<void(glm::uvec2)> callback);
		EXPORT void RegisterCloseCallback(std::function<void(void)> callback);

		void OnResize(const glm::uvec2& size);

		InputState InputState;

	protected:
		Window(const std::string& name, const glm::uvec2& size, bool fullscreen);

		std::vector<std::function<void(glm::uvec2)>> m_resizeCallbacks;
		std::vector<std::function<void(void)>> m_closeCallbacks;
		std::string m_title;
		std::atomic<glm::uvec2> m_size;
		bool m_fullscreen;
		bool m_closed;
	};
}