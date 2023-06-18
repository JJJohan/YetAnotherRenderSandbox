#include "UI.hpp"
#include "Options.hpp"
#include <UI/UIManager.hpp>
#include <Rendering/RenderStats.hpp>
#include <UI/ScrollingGraphBuffer.hpp>
#include <Rendering/Renderer.hpp>

using namespace Engine::Rendering;
using namespace Engine::UI;

namespace Sandbox
{
	UI::UI(Options& options, Renderer* renderer)
		: m_options(options)
		, m_renderer(renderer)
		, m_prevTabIndex(-1)
	{
		m_statGraphBuffers = std::vector<ScrollingGraphBuffer>
		{
			ScrollingGraphBuffer("Scene", 1000),
			ScrollingGraphBuffer("Shadow Cascade 1", 1000),
			ScrollingGraphBuffer("Shadow Cascade 2", 1000),
			ScrollingGraphBuffer("Shadow Cascade 3", 1000),
			ScrollingGraphBuffer("Shadow Cascade 4", 1000),
			ScrollingGraphBuffer("Combine", 1000),
			ScrollingGraphBuffer("TAA", 1000),
			ScrollingGraphBuffer("UI", 1000),
			ScrollingGraphBuffer("Total", 1000)
		};

		m_debugModes = { "None", "Albedo", "Normal", "WorldPos", "MetalRoughness", "Cascade Index" };
	}

	void UI::Draw(const Drawer& drawer)
	{
		if (drawer.Begin("UI"))
		{
			if (drawer.BeginTabBar("##uiTabBar"))
			{
				uint32_t tabIndex;
				if (drawer.BeginTabItem("Options"))
				{
					tabIndex = 0;
					DrawOptions(drawer);
					drawer.EndTabItem();
				}

				if (drawer.BeginTabItem("Statistics"))
				{
					tabIndex = 1;
					DrawStatistics(drawer);
					drawer.EndTabItem();
				}

				if (drawer.BeginTabItem("Render Graph"))
				{
					tabIndex = 2;
					DrawRenderGraph(drawer, tabIndex != m_prevTabIndex);
					drawer.EndTabItem();
				}

				m_prevTabIndex = tabIndex;
				drawer.EndTabBar();
			}

			drawer.End();
		}
	}

	void UI::DrawOptions(const Drawer& drawer)
	{
		int32_t debugMode = static_cast<int32_t>(m_renderer->GetDebugMode());
		if (drawer.ComboBox("Debug Mode", m_debugModes, &debugMode))
		{
			m_renderer->SetDebugMode(debugMode);
		}

		if (drawer.Checkbox("Use Temporal AA", &m_options.UseTAA))
		{
			m_renderer->SetTemporalAAState(m_options.UseTAA);
		}

		bool hdrSupported = m_renderer->IsHDRSupported();
		drawer.BeginDisabled(!hdrSupported);
		if (drawer.Checkbox("Use HDR", &m_options.UseHDR))
		{
			m_renderer->SetHDRState(m_options.UseHDR);
		}
		drawer.EndDisabled();

		if (drawer.Colour3("Clear Colour", m_options.ClearColour))
		{
			m_renderer->SetClearColour(m_options.ClearColour);
		}

		if (drawer.Colour3("Sun Colour", m_options.SunColour))
		{
			m_renderer->SetSunLightColour(m_options.SunColour);
		}

		if (drawer.SliderFloat("Sun Intensity", &m_options.SunIntensity, 0.0f, 20.0f))
		{
			m_renderer->SetSunLightIntensity(m_options.SunIntensity);
		}
	}

	void UI::DrawStatistics(const Drawer& drawer)
	{
		if (drawer.CollapsingHeader("Memory", true))
		{
			const MemoryStats& memoryStats = m_renderer->GetMemoryStats();

			float dedicatedUsage = static_cast<float>(memoryStats.DedicatedUsage) / 1024.0f / 1024.0f;
			float dedicatedBudget = static_cast<float>(memoryStats.DedicatedBudget) / 1024.0f / 1024.0f;
			float dedicatedPercent = dedicatedUsage / dedicatedBudget * 100.0f;
			float sharedUsage = static_cast<float>(memoryStats.SharedUsage) / 1024.0f / 1024.0f;
			float sharedBudget = static_cast<float>(memoryStats.SharedBudget) / 1024.0f / 1024.0f;
			float sharedPercent = sharedUsage / sharedBudget * 100.0f;

			drawer.Text("GBuffer Usage: %.2f MB", static_cast<float>(memoryStats.GBuffer) / 1024.0f / 1024.0f);
			drawer.Text("Shadow Map Usage: %.2f MB", static_cast<float>(memoryStats.ShadowMap) / 1024.0f / 1024.0f);
			drawer.Text("Dedicated VRAM Usage: %.2f MB / %.2f MB (%.2f%%)", dedicatedUsage, dedicatedBudget, dedicatedPercent);
			drawer.Text("Shared VRAM Usage: %.2f MB / %.2f MB (%.2f%%)", sharedUsage, sharedBudget, sharedPercent);
		}

		if (drawer.CollapsingHeader("Performance", true))
		{
			drawer.Text("FPS: %.2f", m_renderer->GetUIManager().GetFPS());

			const std::vector<FrameStats>& statsArray = m_renderer->GetRenderStats();
			if (!statsArray.empty())
			{
				for (size_t pass = 0; pass < statsArray.size(); ++pass)
				{
					const FrameStats& stats = statsArray[pass];
					ScrollingGraphBuffer& buffer = m_statGraphBuffers[pass];
					buffer.AddValue(stats.RenderTime);
				}

				const glm::vec2& space = drawer.GetContentRegionAvailable();
				drawer.PlotGraphs("Frame Times (ms)", m_statGraphBuffers, space);
			}
		}
	}

	void UI::DrawRenderGraph(const Drawer& drawer, bool appearing)
	{
		if (drawer.BeginNodeEditor("Render Graph"))
		{
			drawer.NodeSetupLink("SceneOpaque", "Albedo", "Combine", "Albedo");
			drawer.NodeSetupLink("SceneOpaque", "WorldNormal", "Combine", "WorldNormal");
			drawer.NodeSetupLink("SceneOpaque", "WorldPos", "Combine", "WorldPos");
			drawer.NodeSetupLink("SceneOpaque", "MetalRoughness", "Combine", "MetalRoughness");
			drawer.NodeSetupLink("SceneOpaque", "Velocity", "Combine", "Velocity");
			drawer.NodeSetupLink("SceneShadow", "Shadows", "Combine", "Shadows");

			drawer.DrawNode("SceneOpaque", glm::vec2(-120, -100), {}, { "Albedo", "WorldNormal", "WorldPos", "MetalRoughness", "Velocity" });
			drawer.DrawNode("SceneShadow", glm::vec2(-120, 100), {}, { "Shadows" });
			drawer.DrawNode("Combine", glm::vec2(120, 0), { "Albedo", "WorldNormal", "WorldPos", "MetalRoughness", "Velocity", "Shadows" }, { "FinalColour" });

			if (appearing)
				drawer.NodeEditorZoomToContent();

			drawer.EndNodeEditor();
		}
	}
}