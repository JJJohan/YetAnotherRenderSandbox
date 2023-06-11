#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../Window.hpp"

namespace Engine::OS
{
	class Win32Window : public Window
	{
	public:
		static std::unique_ptr<Win32Window> Create(const std::string& title, const glm::uvec2& size, bool fullscreen);
		Win32Window(const std::string& title, const glm::uvec2& size, bool fullscreen);
		~Win32Window();

		inline virtual void* GetHandle() const override { return m_hWnd; }
		virtual void* GetInstance() const override;

		void OnSizeEvent(uint64_t sizeEventFlag);
		void OnFocusChanged(bool focused);
		virtual void SetCursorVisible(bool visible) override;
		virtual void SetTitle(const std::string& title) override;
		virtual void SetFullscreen(bool fullscreen) override;
		virtual void Resize(const glm::uvec2& size) override;
		virtual void Poll() override;
		virtual void Close() override;
		virtual bool QueryMonitorInfo(MonitorInfo& info) const override;

		void SignalClosed();

	private:
		HWND m_hWnd;
		WINDOWPLACEMENT m_prevPlacement;
		uint64_t m_lastSizeState;
		bool m_cursorVisibleWin32;
	};
}