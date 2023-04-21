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
	{
	}

	bool InputState::KeyDown(KeyCode keyCode) const
	{
		int index = static_cast<int>(keyCode);
		return m_keysDown[index] && !m_keysDownPrevFrame[index];
	}

	bool InputState::KeyPressed(KeyCode keyCode) const
	{
		int index = static_cast<int>(keyCode);
		return m_keysDown[index];
	}

	bool InputState::KeyUp(KeyCode keyCode) const
	{
		int index = static_cast<int>(keyCode);
		return !m_keysDown[index] && m_keysDownPrevFrame[index];
	}

	bool InputState::MouseButtonDown(MouseButton mouseButton) const
	{
		int index = static_cast<int>(mouseButton);
		return m_mouseButtonsDown[index] && !m_mouseButtonsDownPrevFrame[index];
	}

	bool InputState::MouseButtonPressed(MouseButton mouseButton) const
	{
		int index = static_cast<int>(mouseButton);
		return m_mouseButtonsDown[index];
	}

	bool InputState::MouseButtonUp(MouseButton mouseButton) const
	{
		int index = static_cast<int>(mouseButton);
		return !m_mouseButtonsDown[index] && m_mouseButtonsDownPrevFrame[index];
	}

	glm::vec2 InputState::GetMousePos() const
	{
		return m_mousePos;
	}

	glm::vec2 InputState::GetMouseDelta() const
	{
		return m_mouseDelta;
	}

	void InputState::Update()
	{
		m_mouseButtonsDownPrevFrame = m_mouseButtonsDown;
		m_keysDownPrevFrame = m_keysDown;
		m_mouseDelta = glm::vec2();
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

	void InputState::SetMouseDelta(const glm::vec2& delta)
	{
		m_mouseDelta = delta;
	}

	void InputState::SetMousePos(const glm::vec2& pos)
	{
		m_mousePos = pos;
	}
}