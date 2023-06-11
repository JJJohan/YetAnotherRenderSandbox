#include "InputState.hpp"

namespace Engine::OS
{
	InputState::InputState()
		: m_keysDown()
		, m_keysDownPrevFrame()
		, m_mouseButtonsDown()
		, m_mouseButtonsDownPrevFrame()
		, m_mousePos()
		, m_mouseDelta()
		, m_mouseWheelDelta(0.0f)
	{
	}

	void InputState::Update()
	{
		m_mouseButtonsDownPrevFrame = m_mouseButtonsDown;
		m_keysDownPrevFrame = m_keysDown;
		m_mouseDelta = glm::vec2();
		m_mouseWheelDelta = 0.0f;
	}

	void InputState::SetKeyDown(KeyCode keyCode, bool down)
	{
		int index = static_cast<int>(keyCode);
		m_keysDown[index] = down;
	}
	void InputState::SetMouseButtonDown(MouseButton mouseButton, bool down)
	{
		int index = static_cast<int>(mouseButton);
		m_mouseButtonsDown[index] = down;
	}

	void InputState::AddMouseDelta(const glm::vec2& delta)
	{
		m_mouseDelta += delta;
	}

	void InputState::SetMousePos(const glm::vec2& pos)
	{
		m_mousePos = pos;
	}

	void InputState::SetMouseWheelDelta(float mouseWheelDelta)
	{
		m_mouseWheelDelta = mouseWheelDelta;
	}
}