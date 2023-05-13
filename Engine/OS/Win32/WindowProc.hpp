#pragma once

#include "Win32Window.hpp"
#include <windowsx.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
		/*case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			EndPaint(hwnd, &ps);
			return 0;
		}*/
		case WM_SIZE:
		{
			uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);
			window->OnResize(glm::uvec2(width, height));
			window->OnSizeEvent(wParam); // Private variant to handle cursor state.
			break;
		}
		case WM_SETFOCUS:
		{
			window->OnFocusChanged(true);
			break;
		}
		case WM_KILLFOCUS:
		{
			window->OnFocusChanged(false);
			break;
		}
		case WM_MOUSEMOVE:
		{
			int32_t xPos = GET_X_LPARAM(lParam);
			int32_t yPos = GET_Y_LPARAM(lParam);
			window->InputState.SetMousePos(glm::vec2(xPos, yPos));
			break;
		}
		case WM_MOUSEWHEEL:
		{
			int16_t delta = GET_WHEEL_DELTA_WPARAM(wParam);
			window->InputState.SetMouseWheelDelta(static_cast<float>(delta) / 120.0f);
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

		if (window != nullptr && window->IsCursorVisible())
			ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}