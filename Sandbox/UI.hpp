#pragma once

#include <vector>
#include <unordered_map>
#include <chrono>
#include <glm/glm.hpp>

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

		struct QueueTimingsData
		{
			std::vector<std::string> graphicsTooltipLabels;
			std::vector<std::string> graphicsSelectableIds;
			std::vector<glm::vec2> graphicsNormalisedBeginEnds;
			std::vector<std::string> computeTooltipLabels;
			std::vector<std::string> computeSelectableIds;
			std::vector<glm::vec2> computeNormalisedBeginEnds;
			std::chrono::steady_clock::time_point lastUpdateTime;
			float asyncSavingsPercent;
		};

		Engine::Rendering::Renderer* m_renderer;
		Options& m_options;
		std::unordered_map<std::string, Engine::UI::ScrollingGraphBuffer> m_statGraphBuffers;
		std::vector<const char*> m_debugModes;
		std::vector<const char*> m_cullingModes;
		std::vector<const char*> m_shadowResolutions;
		std::vector<const char*> m_aaModes;
		std::vector<const char*> m_nvidiaReflexModes;
		QueueTimingsData m_queueTimingsData;
		uint32_t m_prevTabIndex;
		std::chrono::steady_clock::time_point m_prevTime;
	};
}