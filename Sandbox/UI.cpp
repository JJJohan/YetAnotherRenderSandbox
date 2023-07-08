#include "UI.hpp"
#include "Options.hpp"
#include <Core/Colour.hpp>
#include <UI/UIManager.hpp>
#include <UI/ScrollingGraphBuffer.hpp>
#include <UI/NodeManager.hpp>
#include <Rendering/RenderStats.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Passes/IRenderPass.hpp>

using namespace Engine;
using namespace Engine::Rendering;
using namespace Engine::UI;

namespace Sandbox
{
	UI::UI(Options& options, Renderer* renderer)
		: m_options(options)
		, m_renderer(renderer)
		, m_prevTabIndex(-1)
		, m_statGraphBuffers()
	{
		m_debugModes = { "None", "Albedo", "Normal", "WorldPos", "MetalRoughness", "Cascade Index" };
	}

	void UI::Draw(const Drawer& drawer)
	{
		if (drawer.Begin("UI"))
		{
			if (drawer.BeginTabBar("##uiTabBar"))
			{
				uint32_t tabIndex = std::numeric_limits<uint32_t>::max();
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

			std::unordered_map<const char*, IRenderPass*> passMap(m_renderer->GetRenderGraph().GetPasses());
			passMap.emplace("Total", nullptr);

			// Iterate over the unordered map of graphs by reference and remove any that are no longer in the render graph.
			for (auto it = m_statGraphBuffers.begin(); it != m_statGraphBuffers.end();)
			{
				if (!passMap.contains(it->first))
				{
					it = m_statGraphBuffers.erase(it);
				}
				else
				{
					++it;
				}
			}

			// Iterate over the set and add new map entries if they do not exist.
			for (const auto& pass : passMap)
			{
				if (m_statGraphBuffers.find(pass.first) == m_statGraphBuffers.end())
				{
					m_statGraphBuffers.emplace(pass.first, ScrollingGraphBuffer(pass.first, 1000));
				}
			}

			float total = 0.0f;
			ScrollingGraphBuffer* totalBuffer = nullptr;
			for (auto buffer : m_statGraphBuffers)
			{
				// Skip 'Total' entry
				if (strcmp(buffer.first, "Total") == 0)
				{
					totalBuffer = &buffer.second;
					continue;
				}

				const IRenderPass* pass = passMap[buffer.first];
				float passFrameTime = pass->GetFrameTime();
				buffer.second.AddValue(passFrameTime);
				total += passFrameTime;
			}

			totalBuffer->AddValue(total);

			const glm::vec2& space = drawer.GetContentRegionAvailable();
			drawer.PlotGraphs("Frame Times (ms)", m_statGraphBuffers, space);
		}
	}

	void UI::DrawRenderGraph(const Drawer& drawer, bool appearing)
	{
		if (drawer.BeginNodeEditor("Render Graph"))
		{
			const Colour bufferPinColour(0.5f, 1.0f, 0.5f);
			const Colour imagePinColour(0.2f, 0.5f, 1.0f);

			const std::vector<std::vector<RenderNode>>& graph = m_renderer->GetRenderGraph().GetBuiltGraph();

			// Setup links.
			bool outputLinked = false;
			for (const auto& stage : graph)
			{
				for (const auto& node : stage)
				{
					if (node.Pass == nullptr) 
						continue;

					const char* nodeName = node.Pass->GetName();

					for (const char* input : node.Pass->GetBufferInputs())
					{
						const char* inputNodeName = node.InputBuffers.at(input)->GetName();
						drawer.NodeSetupLink(nodeName, input, inputNodeName, input, bufferPinColour);
					}

					for (const char* input : node.Pass->GetImageInputs())
					{
						const char* inputNodeName = node.InputImages.at(input)->GetName();
						drawer.NodeSetupLink(inputNodeName, input, nodeName, input, imagePinColour);
					}

					if (!outputLinked)
					{
						// Implicitly link output to 'screen' end node.
						for (const char* output : node.Pass->GetImageOutputs())
						{
							if (strcmp(output, "Final") == 0)
							{
								drawer.NodeSetupLink(nodeName, output, "Screen", "Final", imagePinColour);
								outputLinked = true;
								break;
							}
						}
					}
				}
			}

			glm::vec2 offset{};

			// Horizontally place stages.
			for (const auto& stage : graph)
			{
				float stageMaxNodeWidth = 0.0f;

				// Vertically place nodes.
				for (const auto& node : stage)
				{
					bool isPass = node.Pass != nullptr;
					const char* nodeName = node.Resource->GetName();
					Colour nodeColour;

					std::vector<NodePin> inputPins;

					if (isPass)
					{
						for (const char* input : node.Pass->GetBufferInputs())
						{
							inputPins.emplace_back(NodePin(input, bufferPinColour));
						}

						for (const char* input : node.Pass->GetImageInputs())
						{
							inputPins.emplace_back(NodePin(input, imagePinColour));
						}

						nodeColour = Colour(0.5f, 0.5f, 0.5f);
					}
					else
					{
						nodeColour = Colour(0.4f, 0.6f, 0.4f);
					}

					std::vector<NodePin> outputPins;

					for (const char* output : node.Resource->GetBufferOutputs())
					{
						outputPins.emplace_back(NodePin(output, bufferPinColour));
					}

					for (const char* output : node.Resource->GetImageOutputs())
					{
						outputPins.emplace_back(NodePin(output, imagePinColour));
					}

					drawer.DrawNode(nodeName, offset, inputPins, outputPins, nodeColour);
					glm::vec2 nodeSize = drawer.GetNodeSize(nodeName);

					stageMaxNodeWidth = std::max(stageMaxNodeWidth, nodeSize.x);
					offset.y += nodeSize.y + 50.0f;
				}

				offset.y = 0.0f;
				offset.x += stageMaxNodeWidth + 50.0f;
			}

			// Draw 'Screen' node.
			drawer.DrawNode("Screen", offset, { NodePin("Final", imagePinColour) }, {}, imagePinColour);

			if (appearing)
				drawer.NodeEditorZoomToContent();

			drawer.EndNodeEditor();
		}
	}
}