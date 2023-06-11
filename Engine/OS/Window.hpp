#pragma once

#include <string>
#include <memory>
#include "Core/Macros.hpp"
#include "InputState.hpp"
#include <functional>

namespace Engine::OS
{
	struct MonitorInfo
	{
		std::string DeviceName;
		uint32_t BitsPerColor;
		float RedPrimary[2];
		float GreenPrimary[2];
		float BluePrimary[2];
		float WhitePoint[2];
		float MinLuminance;
		float MaxLuminance;
		float MaxFullFrameLuminance;
	};

	class Window
	{
	public:
		EXPORT static std::unique_ptr<Window> Create(const std::string& title, const glm::uvec2& size, bool fullscreen);
		EXPORT virtual ~Window();

		inline bool IsCursorVisible() const { return m_cursorVisible; }
		inline const std::string& GetTitle() const { return m_title; }
		inline const glm::uvec2 GetSize() const { return m_size; }
		inline bool IsFullscreen() const { return m_fullscreen; }
		inline bool IsClosed() const { return m_closed; }
		inline virtual void* GetHandle() const { return nullptr; }
		inline virtual void* GetInstance() const { return nullptr; }

		inline virtual void SetCursorVisible(bool visible) { m_cursorVisible = visible; }
		inline virtual void SetTitle(const std::string& title) { m_title = title; }
		inline virtual void SetFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }
		EXPORT virtual void Resize(const glm::uvec2& size);
		EXPORT virtual void Close();
		EXPORT virtual void Poll();
		EXPORT virtual bool QueryMonitorInfo(MonitorInfo& info) const;

		EXPORT void RegisterResizeCallback(std::function<void(const glm::uvec2&)> callback);
		EXPORT void UnregisterResizeCallback(std::function<void(const glm::uvec2&)> callback);

		EXPORT void RegisterCloseCallback(std::function<void(void)> callback);
		EXPORT void UnregisterCloseCallback(std::function<void(void)> callback);

		void OnResize(const glm::uvec2& size);

		InputState InputState;

	protected:
		Window(const std::string& name, const glm::uvec2& size, bool fullscreen);

		std::vector<std::function<void(const glm::uvec2&)>> m_resizeCallbacks;
		std::vector<std::function<void(void)>> m_closeCallbacks;
		std::string m_title;
		std::atomic<glm::uvec2> m_size;
		bool m_fullscreen;
		bool m_closed;
		bool m_cursorVisible;
	};
}