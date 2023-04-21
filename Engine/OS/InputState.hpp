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

		EXPORT bool KeyDown(KeyCode keyCode) const;
		EXPORT bool KeyPressed(KeyCode keyCode) const;
		EXPORT bool KeyUp(KeyCode keyCode) const;

		EXPORT bool MouseButtonDown(MouseButton mouseButton) const;
		EXPORT bool MouseButtonPressed(MouseButton mouseButton) const;
		EXPORT bool MouseButtonUp(MouseButton mouseButton) const;

		EXPORT glm::vec2 GetMousePos() const;
		EXPORT glm::vec2 GetMouseDelta() const;

		void Update();
		void SetKeyDown(KeyCode keyCode, bool down);
		void SetMouseButtonDown(MouseButton mouseButton, bool down);
		void SetMouseDelta(const glm::vec2& delta);
		void SetMousePos(const glm::vec2& pos);

	private:
		std::array<bool, 256> m_keysDown;
		std::array<bool, 256> m_keysDownPrevFrame;
		std::array<bool, 3> m_mouseButtonsDown;
		std::array<bool, 3> m_mouseButtonsDownPrevFrame;
		glm::vec2 m_mousePos;
		glm::vec2 m_mouseDelta;
	};
}