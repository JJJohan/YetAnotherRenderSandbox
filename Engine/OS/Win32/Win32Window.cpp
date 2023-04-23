#include <Core/Logging/Logger.hpp>
#include "Win32Window.hpp"
#include <vector>

using namespace Engine::Logging;

namespace Engine::OS
{
	LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		Win32Window* window = (Win32Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

		switch (uMsg)
		{
		case WM_NCCREATE:
		{
			CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
			window = (Win32Window*)pCreate->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
			break;
		}
		case WM_DESTROY:
		{
			window->SignalClosed();
			PostQuitMessage(0);
			return 0;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_SIZE:
		{
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			window->NotifyResizeEvent(width, height);
			break;
		}
		case WM_INPUT:
		{
			UINT dataSize = 0;
			GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));

			if (dataSize > 0)
			{
				std::vector<BYTE> rawdata(dataSize);

				if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawdata.data(), &dataSize, sizeof(RAWINPUTHEADER)) == dataSize)
				{
					RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(rawdata.data());
					if (raw->header.dwType == RIM_TYPEMOUSE)
					{
						RAWMOUSE& rawMouse = raw->data.mouse;
						if ((rawMouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE)
						{
							bool isVirtualDesktop = (rawMouse.usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;

							int width = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
							int height = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

							int absoluteX = int((rawMouse.lLastX / 65535.0f) * width);
							int absoluteY = int((rawMouse.lLastY / 65535.0f) * height);

							window->InputState.SetMousePos(glm::vec2(absoluteX, absoluteY));
						}
						else if ((rawMouse.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE)
						{
							window->InputState.SetMouseDelta(glm::vec2(rawMouse.lLastX, rawMouse.lLastY));
						}

						if ((rawMouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) == RI_MOUSE_LEFT_BUTTON_DOWN)
						{
							window->InputState.SetMouseButtonDown(MouseButton::LEFT, true);
						}
						else if ((rawMouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) == RI_MOUSE_LEFT_BUTTON_UP)
						{
							window->InputState.SetMouseButtonDown(MouseButton::LEFT, false);
						}

						if ((rawMouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) == RI_MOUSE_MIDDLE_BUTTON_DOWN)
						{
							window->InputState.SetMouseButtonDown(MouseButton::MIDDLE, true);
						}
						else if ((rawMouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) == RI_MOUSE_MIDDLE_BUTTON_UP)
						{
							window->InputState.SetMouseButtonDown(MouseButton::MIDDLE, false);
						}

						if ((rawMouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) == RI_MOUSE_RIGHT_BUTTON_DOWN)
						{
							window->InputState.SetMouseButtonDown(MouseButton::RIGHT, true);
						}
						else if ((rawMouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) == RI_MOUSE_RIGHT_BUTTON_UP)
						{
							window->InputState.SetMouseButtonDown(MouseButton::RIGHT, false);
						}
					}
					else if (raw->header.dwType == RIM_TYPEKEYBOARD)
					{
						window->InputState.SetKeyDown(static_cast<KeyCode>(raw->data.keyboard.VKey), raw->data.keyboard.Flags & RI_KEY_BREAK ? false : true);
					}
				}
			}
			return 0;
		}
		}

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	Win32Window::Win32Window(const std::string& title, uint32_t width, uint32_t height, bool fullscreen)
		: Window(title, width, height, fullscreen)
		, m_hWnd(nullptr)
		, m_prevPlacement()
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

	std::unique_ptr<Win32Window> Win32Window::Create(const std::string& title, uint32_t width, uint32_t height, bool fullscreen)
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
		WindowRect.left = (long)(center_width - width / 2); // Set Left Value To 0
		WindowRect.right = (long)(center_width + width / 2); // Set Right Value To Requested Width
		WindowRect.top = (long)(center_height - height / 2); // Set Top Value To 0
		WindowRect.bottom = (long)(center_height + height / 2); // Set Bottom Value To Requested Height

		WNDCLASS wc = {};
		wc.hInstance = hInstance;
		wc.lpszClassName = L"Engine Window Class";
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;   // Redraw On Size, And Own DC For Window.
		wc.lpfnWndProc = WndProc;                        // WndProc Handles Messages
		wc.cbClsExtra = 0;                               // No Extra Window Data
		wc.cbWndExtra = 0;                               // No Extra Window Data
		wc.hInstance = hInstance;                      // Set The Instance
		wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);          // Load The Default Icon
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);        // Load The Arrow Pointer
		wc.hbrBackground = NULL;                         // No Background Required
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
			dmScreenSettings.dmPelsWidth = width;                   // Selected Screen Width
			dmScreenSettings.dmPelsHeight = height;                 // Selected Screen Height
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

		DWORD dwExStyle;
		DWORD dwStyle;
		if (fullscreen)
		{
			dwExStyle = WS_EX_APPWINDOW;  // Window Extended Style
			dwStyle = WS_POPUP;           // Windows Style
			ShowCursor(FALSE);            // Hide Mouse Pointer
		}
		else
		{
			dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE; // Window Extended Style
			dwStyle = WS_OVERLAPPEDWINDOW;                  // Windows Style
		}

		AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);   // Adjust Window To True Requested Size

		std::unique_ptr<Win32Window> window = std::make_unique<Win32Window>(title, width, height, fullscreen);

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
		Close();
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
						MONITOR_DEFAULTTOPRIMARY), &mi)) {
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

	void Win32Window::Resize(uint32_t width, uint32_t height)
	{
		if (m_hWnd && (m_width != width || m_height != height))
		{
			if (SetWindowPos(m_hWnd, 0, 0, 0, width, height, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOMOVE) == 0)
			{
				Logger::Log(LogLevel::WARNING, "Failed to resize window to new size of ({0}, {1}).", width, height);
			}
		}

		Window::Resize(width, height);
	}
}