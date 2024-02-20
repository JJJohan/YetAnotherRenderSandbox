#pragma once

#include <vector>
#include <unordered_map>
#include <chrono>

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
		std::unordered_map<std::string, Engine::UI::ScrollingGraphBuffer> m_statGraphBuffers;
		std::vector<const char*> m_debugModes;
		std::vector<const char*> m_cullingModes;
		uint32_t m_prevTabIndex;
		std::chrono::steady_clock::time_point m_prevTime;
	};
}