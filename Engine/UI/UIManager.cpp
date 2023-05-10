#include "UIManager.hpp"
#include "Rendering/Renderer.hpp"
#include "OS/Window.hpp"

using namespace Engine::OS;
using namespace Engine::Rendering;

namespace Engine::UI
{
	UIManager::UIManager(const Window& window, Renderer& renderer)
		: m_window(window)
		, m_renderer(renderer)
	{
	}

	UIManager::~UIManager()
	{
	}
}