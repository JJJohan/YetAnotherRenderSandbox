#include "UIManager.hpp"
#include "Rendering/Renderer.hpp"
#include "OS/Window.hpp"
#include "Core/Logging/Logger.hpp"
#include "imgui.h"

using namespace Engine::OS;
using namespace Engine::Rendering;
using namespace Engine::Logging;

namespace Engine::UI
{
	UIManager::UIManager(const Window& window, Renderer& renderer)
		: m_window(window)
		, m_renderer(renderer)
		, m_drawCallbacks()
		, m_drawer()
	{
	}

	UIManager::~UIManager()
	{
	}

	void UIManager::RegisterDrawCallback(std::function<void(const Drawer&)> callback)
	{
		const auto& target = callback.target<void(const Drawer&)>();
		for (auto it = m_drawCallbacks.cbegin(); it != m_drawCallbacks.cend(); ++it)
		{
			if (it->target<void(const Drawer&)>() == target)
			{
				Logger::Error("Callback already registered.");
				return;
			}
		}

		m_drawCallbacks.push_back(callback);
	}

	void UIManager::UnregisterDrawCallback(std::function<void(const Drawer&)> callback)
	{
		const auto& target = callback.target<void(const Drawer&)>();
		for (auto it = m_drawCallbacks.cbegin(); it != m_drawCallbacks.cend(); ++it)
		{
			if (it->target<void(const Drawer&)>() == target)
			{
				m_drawCallbacks.erase(it);
				return;
			}
		}

		Logger::Error("Callback was not registered.");
	}

	float UIManager::GetFPS() const
	{
		return ImGui::GetIO().Framerate;
	}
}