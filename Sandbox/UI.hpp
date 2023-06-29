#pragma once

#include <vector>
#include <unordered_map>

namespace Engine::UI
{
	class Drawer;
	struct ScrollingGraphBuffer;
}

namespace Engine::Rendering
{
	class Renderer;
}

namespace Sandbox
{
	struct Options;

	class UI
	{
	public:
		UI(Options& options, Engine::Rendering::Renderer* renderer);
		void Draw(const Engine::UI::Drawer& drawer);

	private:
		void DrawOptions(const Engine::UI::Drawer& drawer);
		void DrawStatistics(const Engine::UI::Drawer& drawer);
		void DrawRenderGraph(const Engine::UI::Drawer& drawer, bool appearing);

		Engine::Rendering::Renderer* m_renderer;
		Options& m_options;
		std::unordered_map<const char*, Engine::UI::ScrollingGraphBuffer> m_statGraphBuffers;
		std::vector<const char*> m_debugModes;
		uint32_t m_prevTabIndex;
	};
}