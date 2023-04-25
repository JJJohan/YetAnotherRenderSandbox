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

		virtual void* GetHandle() const;
		virtual void* GetInstance() const;
		virtual void SetTitle(const std::string& title);
		virtual void SetFullscreen(bool fullscreen);
		virtual void Resize(const glm::uvec2& size);
		virtual void Poll();
		virtual void Close();
		void SignalClosed();

	private:
		HWND m_hWnd;
		WINDOWPLACEMENT m_prevPlacement;
	};
}