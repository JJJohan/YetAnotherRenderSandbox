#pragma once

#include "InputEnums.hpp"
#include "Core/Macros.hpp"
#include <glm/vec2.hpp>
#include <array>

namespace Engine::OS
{
	class InputState
	{
	public:
		InputState();

		inline bool KeyDown(KeyCode keyCode) const
		{
			int index = static_cast<int>(keyCode);
			return m_keysDown[index] && !m_keysDownPrevFrame[index];
		};

		inline bool KeyPressed(KeyCode keyCode) const
		{
			int index = static_cast<int>(keyCode);
			return m_keysDown[index];
		};

		inline bool KeyUp(KeyCode keyCode) const
		{
			int index = static_cast<int>(keyCode);
			return !m_keysDown[index] && m_keysDownPrevFrame[index];
		};

		inline bool MouseButtonDown(MouseButton mouseButton) const
		{
			int index = static_cast<int>(mouseButton);
			return m_mouseButtonsDown[index] && !m_mouseButtonsDownPrevFrame[index];
		};

		inline bool MouseButtonPressed(MouseButton mouseButton) const
		{
			int index = static_cast<int>(mouseButton);
			return m_mouseButtonsDown[index];
		};

		inline bool MouseButtonUp(MouseButton mouseButton) const
		{
			int index = static_cast<int>(mouseButton);
			return !m_mouseButtonsDown[index] && m_mouseButtonsDownPrevFrame[index];
		};

		inline glm::vec2 GetMousePos() const { return m_mousePos; }
		inline glm::vec2 GetMouseDelta() const { return m_mouseDelta; }
		inline float GetMouseWheelDelta() const { return m_mouseWheelDelta; }

		void Update();
		void SetKeyDown(KeyCode keyCode, bool down);
		void SetMouseButtonDown(MouseButton mouseButton, bool down);
		void AddMouseDelta(const glm::vec2& delta);
		void SetMousePos(const glm::vec2& pos);
		void SetMouseWheelDelta(float mouseWheelDelta);

	private:
		std::array<bool, 256> m_keysDown;
		std::array<bool, 256> m_keysDownPrevFrame;
		std::array<bool, 3> m_mouseButtonsDown;
		std::array<bool, 3> m_mouseButtonsDownPrevFrame;
		glm::vec2 m_mousePos;
		glm::vec2 m_mouseDelta;
		float m_mouseWheelDelta;
	};
}