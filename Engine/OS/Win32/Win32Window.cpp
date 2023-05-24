#include <Core/Logging/Logger.hpp>
#include "Win32Window.hpp"
#include <vector>
#include "WindowProc.hpp"
#include <dxgi1_6.h>
#include <strsafe.h>

#pragma comment(lib, "Dxgi")

using namespace Engine::Logging;

namespace Engine::OS
{
	std::string WStringToString(LPCWSTR wideString)
	{
		std::wstring wstr(wideString);
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string str(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
		return str;
	}

	void LogWin32Error()
	{
		// Retrieve the system error message for the last-error code
		LPVOID lpMsgBuf;
		LPVOID lpDisplayBuf;
		DWORD dw = GetLastError();

		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL);

		// Display the error message and exit the process
		lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
			(lstrlen((LPCTSTR)lpMsgBuf) + 40) * sizeof(TCHAR));
		StringCchPrintf((LPTSTR)lpDisplayBuf,
			LocalSize(lpDisplayBuf) / sizeof(TCHAR),
			TEXT("Win32 error %d: %s"), dw, lpMsgBuf);

		std::string str = WStringToString((LPCTSTR)lpDisplayBuf);
		Logger::Error(str);

		LocalFree(lpMsgBuf);
		LocalFree(lpDisplayBuf);
	}

	Win32Window::Win32Window(const std::string& title, const glm::uvec2& size, bool fullscreen)
		: Window(title, size, fullscreen)
		, m_hWnd(nullptr)
		, m_prevPlacement()
		, m_lastSizeState(0)
		, m_cursorVisibleWin32(true)
	{
	}

	void* Win32Window::GetHandle() const
	{
		return m_hWnd;
	}

	void* Win32Window::GetInstance() const
	{
		return GetModuleHandle(nullptr);
	}

	void Win32Window::OnSizeEvent(uint64_t currentState)
	{
		if (m_lastSizeState == currentState)
			return;

		uint64_t prevState = m_lastSizeState;
		m_lastSizeState = currentState;
		if (prevState == SIZE_MINIMIZED)
		{
			// Keep cursor visibility in sync.
			SetCursorVisible(m_cursorVisible);
		}
		else if (currentState == SIZE_MINIMIZED && !m_cursorVisibleWin32)
		{
			// Unhide cursor if window was somehow minimized with cursor hidden.
			::ShowCursor(true);
			::ClipCursor(nullptr);
			m_cursorVisibleWin32 = true;
		}
	}

	void Win32Window::OnFocusChanged(bool focused)
	{
		if (focused)
		{
			SetCursorVisible(m_cursorVisible);
		}
		else if (!m_cursorVisibleWin32)
		{
			::ShowCursor(true);
			::ClipCursor(nullptr);
			m_cursorVisibleWin32 = true;
		}
	}

	std::unique_ptr<Win32Window> Win32Window::Create(const std::string& title, const glm::uvec2& size, bool fullscreen)
	{
		std::wstring title_w = std::wstring(title.begin(), title.end());
		LPCWSTR window_title = title_w.c_str();

		HMODULE hInstance = GetModuleHandle(nullptr);
		if (hInstance == nullptr)
		{
			Logger::Log(LogLevel::FATAL, "Failed to get module handle.");
			return nullptr;
		}

		int center_width = GetSystemMetrics(SM_CXSCREEN) / 2;
		int center_height = GetSystemMetrics(SM_CYSCREEN) / 2;

		RECT WindowRect;
		WindowRect.left = (long)(center_width - size.x / 2); // Set Left Value To 0
		WindowRect.right = (long)(center_width + size.x / 2); // Set Right Value To Requested Width
		WindowRect.top = (long)(center_height - size.y / 2); // Set Top Value To 0
		WindowRect.bottom = (long)(center_height + size.y / 2); // Set Bottom Value To Requested Height

		WNDCLASS wc = {};
		wc.hInstance = hInstance;
		wc.lpszClassName = L"Engine Window Class";
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;   // Redraw On Size, And Own DC For Window.
		wc.lpfnWndProc = WndProc;                        // WndProc Handles Messages
		wc.cbClsExtra = 0;                               // No Extra Window Data
		wc.cbWndExtra = 0;                               // No Extra Window Data
		wc.hInstance = hInstance;                        // Set The Instance
		wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);       // Load The Default Icon
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);     // Load The Arrow Pointer
		wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH); // No Background Required
		wc.lpszMenuName = NULL;                          // We Don't Want A Menu

		if (!RegisterClass(&wc))
		{
			Logger::Log(LogLevel::FATAL, "Failed to register window class.");
			MessageBox(nullptr, L"Failed to register window class.", L"Error", MB_OK | MB_ICONEXCLAMATION);
			return nullptr;
		}

		if (fullscreen)
		{
			DEVMODE dmScreenSettings;                               // Device Mode
			memset(&dmScreenSettings, 0, sizeof(dmScreenSettings)); // Makes Sure Memory's Cleared
			dmScreenSettings.dmSize = sizeof(dmScreenSettings);     // Size Of The Devmode Structure
			dmScreenSettings.dmPelsWidth = size.x;                  // Selected Screen Width
			dmScreenSettings.dmPelsHeight = size.y;                 // Selected Screen Height
			dmScreenSettings.dmBitsPerPel = 32;                     // Selected Bits Per Pixel
			dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			// Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
			if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				// If The Mode Fails, Offer Two Options.  Quit Or Use Windowed Mode.
				Logger::Log(LogLevel::WARNING, "Fullscreen mode not supported, querying for window mode fallback.");
				if (MessageBox(nullptr, L"The Requested Fullscreen Mode Is Not Supported By\nYour Video Card. Use Windowed Mode Instead?", L"Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
				{
					fullscreen = FALSE;
				}
				else
				{
					return nullptr;
				}
			}
		}

		std::unique_ptr<Win32Window> window = std::make_unique<Win32Window>(title, size, fullscreen);

		DWORD dwExStyle;
		DWORD dwStyle;
		if (fullscreen)
		{
			dwExStyle = WS_EX_APPWINDOW;  // Window Extended Style
			dwStyle = WS_POPUP;           // Windows Style
			window->SetCursorVisible(false);
		}
		else
		{
			dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE; // Window Extended Style
			dwStyle = WS_OVERLAPPEDWINDOW;                  // Windows Style
		}

		AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);   // Adjust Window To True Requested Size

		// Create The Window
		if (!(window->m_hWnd = CreateWindowEx(dwExStyle, // Extended Style For The Window
			wc.lpszClassName,                    // Class Name
			window_title,                        // Window Title
			dwStyle |                            // Defined Window Style
			WS_CLIPSIBLINGS |                    // Required Window Style
			WS_CLIPCHILDREN,                     // Required Window Style
			WindowRect.left, WindowRect.top,     // Window Position
			WindowRect.right - WindowRect.left,  // Calculate Window Width
			WindowRect.bottom - WindowRect.top,  // Calculate Window Height
			nullptr,                                // No Parent Window
			nullptr,                                // No Menu
			hInstance,                           // Instance
			window.get())))                      // Pass reference of this to WM_CREATE
		{
			Logger::Log(LogLevel::FATAL, "Failed to create window.");
			MessageBox(nullptr, L"Failed to create window.", L"Error", MB_OK | MB_ICONEXCLAMATION);
			return nullptr;
		}

		ShowWindow(window->m_hWnd, SW_SHOW); // Show The Window
		SetForegroundWindow(window->m_hWnd); // Slightly Higher Priority
		SetFocus(window->m_hWnd);            // Sets Keyboard Focus To The Window

		// Register raw input
		RAWINPUTDEVICE Rid[2];

		Rid[0].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
		Rid[0].usUsage = 0x02;              // HID_USAGE_GENERIC_MOUSE
		Rid[0].dwFlags = 0;
		Rid[0].hwndTarget = 0;

		Rid[1].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
		Rid[1].usUsage = 0x06;              // HID_USAGE_GENERIC_KEYBOARD
		Rid[1].dwFlags = 0;
		Rid[1].hwndTarget = 0;

		if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
		{
			Logger::Log(LogLevel::FATAL, "Failed to register raw input for keyboard and mouse.");
			MessageBox(nullptr, L"Failed to register raw input for keyboard and mouse.", L"Error", MB_OK | MB_ICONEXCLAMATION);
		}

		return window;
	}

	Win32Window::~Win32Window()
	{
	}

	void Win32Window::Poll()
	{
		Window::Poll();

		MSG msg = {};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	void Win32Window::Close()
	{
		Window::Close();

		if (m_hWnd != nullptr)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
		}
	}

	void Win32Window::SignalClosed()
	{
		Window::Close();

		m_hWnd = nullptr;
	}

	bool Win32Window::QueryMonitorInfo(MonitorInfo& info) const
	{
		if (m_hWnd == nullptr)
		{
			Logger::Error("Window needs to be created to query monitor info.");
			return false;
		}

		HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
		if (monitor == nullptr)
		{
			Logger::Error("No monitor found.");
			return false;
		}

		IDXGIFactory4* dxgiFactory = nullptr;
		if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
		{
			Logger::Error("Could not create DXGI factory.");
			return false;
		}

		bool infoAcquired = false;
		IDXGIAdapter1* dxgiAdapter = nullptr;
		uint32_t adapterIndex = 0;
		while (dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			// TODO: Match correct adapter to output that window is on?
			uint32_t outputIndex = 0;
			IDXGIOutput* tmpDxgiOutput = nullptr;
			while (dxgiAdapter->EnumOutputs(outputIndex, &tmpDxgiOutput) != DXGI_ERROR_NOT_FOUND)
			{
				IDXGIOutput6* dxgiOutput = nullptr;
				if (tmpDxgiOutput->QueryInterface(IID_PPV_ARGS(&dxgiOutput)) == S_OK)
				{
					DXGI_OUTPUT_DESC1 desc;
					if (dxgiOutput->GetDesc1(&desc) == S_OK)
					{
						infoAcquired = true;
						info.DeviceName = WStringToString(desc.DeviceName);
						info.BitsPerColor = desc.BitsPerColor;
						info.RedPrimary[0] = desc.RedPrimary[0];
						info.RedPrimary[1] = desc.RedPrimary[1];
						info.GreenPrimary[0] = desc.GreenPrimary[0];
						info.GreenPrimary[1] = desc.GreenPrimary[1];
						info.BluePrimary[0] = desc.BluePrimary[0];
						info.BluePrimary[1] = desc.BluePrimary[1];
						info.WhitePoint[0] = desc.WhitePoint[0];
						info.WhitePoint[1] = desc.WhitePoint[1];
						info.MinLuminance = desc.MinLuminance;
						info.MaxLuminance = desc.MaxLuminance;
						info.MaxFullFrameLuminance = desc.MaxFullFrameLuminance;
					}
				}
				//++outputIndex;
				break;
			}

			dxgiAdapter->Release();
			//++adapterIndex;
			break;
		}

		dxgiFactory->Release();

		return infoAcquired;
	}

	void Win32Window::SetTitle(const std::string& title)
	{
		if (m_hWnd && m_title != title)
		{
			std::wstring title_w = std::wstring(title.begin(), title.end());
			if (SetWindowText(m_hWnd, title_w.c_str()) == 0)
			{
				Logger::Log(LogLevel::WARNING, "Failed to set window title to '{}'.", title);
			}
		}

		Window::SetTitle(title);
	}

	void Win32Window::SetCursorVisible(bool visible)
	{
		Window::SetCursorVisible(visible);

		if (m_cursorVisibleWin32 != visible)
		{
			::ShowCursor(visible);
			m_cursorVisibleWin32 = visible;
		}

		if (visible)
		{
			::ClipCursor(nullptr);
		}
		else
		{
			RECT rect = {};
			::GetWindowRect(m_hWnd, &rect);
			::ClipCursor(&rect);
		}
	}

	void Win32Window::SetFullscreen(bool fullscreen)
	{
		if (m_hWnd && m_fullscreen != fullscreen)
		{
			if (fullscreen)
			{
				DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);

				MONITORINFO mi = { sizeof(mi) };
				if (GetWindowPlacement(m_hWnd, &m_prevPlacement) &&
					GetMonitorInfo(MonitorFromWindow(m_hWnd,
						MONITOR_DEFAULTTOPRIMARY), &mi))
				{
					SetWindowLong(m_hWnd, GWL_STYLE,
						dwStyle & ~WS_OVERLAPPEDWINDOW);
					SetWindowPos(m_hWnd, HWND_TOP,
						mi.rcMonitor.left, mi.rcMonitor.top,
						mi.rcMonitor.right - mi.rcMonitor.left,
						mi.rcMonitor.bottom - mi.rcMonitor.top,
						SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
				}
				else
				{
					SetWindowLong(m_hWnd, GWL_STYLE,
						dwStyle | WS_OVERLAPPEDWINDOW);
					SetWindowPlacement(m_hWnd, &m_prevPlacement);
					SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
						SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
						SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
				}
			}
		}

		Window::SetFullscreen(fullscreen);
	}

	void Win32Window::Resize(const glm::uvec2& size)
	{
		glm::uvec2 currentSize = m_size;
		if (m_hWnd && currentSize != size)
		{
			if (SetWindowPos(m_hWnd, 0, 0, 0, size.x, size.y, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOMOVE) == 0)
			{
				Logger::Log(LogLevel::WARNING, "Failed to resize window to new size of ({0}, {1}).", size.x, size.y);
			}
		}

		Window::Resize(size);
	}
}