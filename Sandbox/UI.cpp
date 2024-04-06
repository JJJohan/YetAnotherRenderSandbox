#include "UI.hpp"
#include "Options.hpp"
#include <Core/Colour.hpp>
#include <UI/UIManager.hpp>
#include <UI/ScrollingGraphBuffer.hpp>
#include <UI/NodeManager.hpp>
#include <Rendering/RenderStats.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/RenderPasses/IRenderPass.hpp>
#include <Rendering/ComputePasses/IComputePass.hpp>

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
		, m_prevTime()
	{
		m_debugModes = { "None", "Albedo", "Normal", "WorldPos", "MetalRoughness", "Cascade Index" };
		m_cullingModes = { "Paused", "None", "Frustum", "Frustum + Occlusion" };
		m_shadowResolutions = { "1024", "2048", "4096" };
		m_aaModes = { "None", "FXAA", "SMAA", "TAA" };
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

	uint32_t shadowResolutionValues[3] = { 1024, 2048, 4096 };

	void UI::DrawOptions(const Drawer& drawer)
	{
		int32_t debugMode = static_cast<int32_t>(m_renderer->GetDebugMode());
		if (drawer.ComboBox("Debug Mode", m_debugModes, &debugMode))
		{
			m_renderer->SetDebugMode(debugMode);
		}

		int32_t aaMode = static_cast<int32_t>(m_options.AntiAliasingMode);
		if (drawer.ComboBox("Anti Aliasing", m_aaModes, &aaMode))
		{
			m_options.AntiAliasingMode = static_cast<AntiAliasingMode>(aaMode);
			m_renderer->SetAntiAliasingMode(m_options.AntiAliasingMode);
		}

		int32_t cullingMode = static_cast<int32_t>(m_options.CullingMode);
		if (drawer.ComboBox("Culling Mode", m_cullingModes, &cullingMode))
		{
			m_options.CullingMode = static_cast<CullingMode>(cullingMode);
			m_renderer->SetCullingMode(m_options.CullingMode);
		}

		if (drawer.ComboBox("Shadow Resolution", m_shadowResolutions, &m_options.ShadowResolutionIndex))
		{
			m_renderer->SetShadowResolution(shadowResolutionValues[m_options.ShadowResolutionIndex]);
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

			for (const auto& resource : memoryStats.ResourceMemoryUsage)
			{
				drawer.Text("%s Usage: %.2f MB", resource.first.c_str(), static_cast<float>(resource.second) / 1024.0f / 1024.0f);
			}

			drawer.Text("Dedicated VRAM Usage: %.2f MB / %.2f MB (%.2f%%)", dedicatedUsage, dedicatedBudget, dedicatedPercent);
			drawer.Text("Shared VRAM Usage: %.2f MB / %.2f MB (%.2f%%)", sharedUsage, sharedBudget, sharedPercent);
		}

		if (drawer.CollapsingHeader("Performance", true))
		{
			auto currentTime = std::chrono::high_resolution_clock::now();
			float cpuFrameTime = std::chrono::duration<float, std::chrono::milliseconds::period>(currentTime - m_prevTime).count();
			float gpuFrameTime = 0.0f;
			m_prevTime = currentTime;

			float fps = m_renderer->GetUIManager().GetFPS();

			const std::unordered_map<std::string, FrameStats>& frameStats = m_renderer->GetRenderStats();

			// Iterate over the unordered map of graphs by reference and remove any that are no longer in the render graph.
			for (auto it = m_statGraphBuffers.begin(); it != m_statGraphBuffers.end();)
			{
				if (!frameStats.contains(it->first))
				{
					it = m_statGraphBuffers.erase(it);
				}
				else
				{
					++it;
				}
			}

			// Iterate over the set and add new map entries if they do not exist.
			for (const auto& pass : frameStats)
			{
				if (m_statGraphBuffers.find(pass.first) == m_statGraphBuffers.end())
				{
					m_statGraphBuffers.emplace(pass.first, ScrollingGraphBuffer(pass.first, 1000));
				}

				if (gpuFrameTime < 1e-5f && pass.first == "Total")
				{
					gpuFrameTime = pass.second.RenderTime;
				}
			}

			drawer.Text("FPS %.2f", fps);
			drawer.Text("GPU Frame Time %.2fms", gpuFrameTime);
			drawer.Text("CPU Frame Time: %.2fms", cpuFrameTime);

			for (auto& buffer : m_statGraphBuffers)
			{
				const FrameStats& stats = frameStats.at(buffer.first);
				float passFrameTime = stats.RenderTime;
				buffer.second.AddValue(passFrameTime);
			}

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

			const std::vector<std::vector<RenderGraphNode>>& graph = m_renderer->GetRenderGraph().GetBuiltGraph();

			// Setup links.
			bool outputLinked = false;
			for (auto stage = graph.rbegin(); stage != graph.rend(); ++stage)
			{
				for (const auto& node : *stage)
				{
					const char* nodeName = node.Node->GetName().c_str();

					for (const auto& input : node.Node->GetBufferInputs())
					{
						const char* inputNodeName = node.InputBufferSources.at(input.first).Node->GetName().c_str();
						const char* inputCstr = input.first.c_str();
						drawer.NodeSetupLink(inputNodeName, inputCstr, nodeName, inputCstr, bufferPinColour);
					}

					for (const auto& input : node.Node->GetImageInputInfos())
					{
						const char* inputNodeName = node.InputImageSources.at(input.first).Node->GetName().c_str();
						const char* inputCstr = input.first.c_str();
						drawer.NodeSetupLink(inputNodeName, inputCstr, nodeName, inputCstr, imagePinColour);
					}

					if (!outputLinked)
					{
						// Implicitly link output to 'screen' end node.
						for (const auto& output : node.Node->GetImageOutputInfos())
						{
							if (output.first == "Output")
							{
								const char* outputCstr = output.first.c_str();
								drawer.NodeSetupLink(nodeName, outputCstr, "Screen", "Output", imagePinColour);
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
					const char* nodeName = node.Node->GetName().c_str();
					Colour nodeColour;

					std::vector<NodePin> inputPins;
					for (const auto& input : node.Node->GetBufferInputs())
					{
						inputPins.emplace_back(NodePin(input.first, bufferPinColour));
					}

					for (const auto& input : node.Node->GetImageInputInfos())
					{
						inputPins.emplace_back(NodePin(input.first, imagePinColour));
					}

					std::vector<NodePin> outputPins;
					for (const auto& output : node.Node->GetBufferOutputs())
					{
						outputPins.emplace_back(NodePin(output.first, bufferPinColour));
					}

					for (const auto& output : node.Node->GetImageOutputInfos())
					{
						outputPins.emplace_back(NodePin(output.first, imagePinColour));
					}

					if (node.Type == RenderNodeType::Pass)
					{
						nodeColour = Colour(0.5f, 0.5f, 0.5f); // Gray
					}
					else if (node.Type == RenderNodeType::Compute)
					{
						nodeColour = Colour(0.3f, 0.4f, 0.6f); // Blue-ish
					}
					else
					{
						nodeColour = Colour(0.4f, 0.6f, 0.4f); // Green-ish
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
			drawer.DrawNode("Screen", offset, { NodePin("Output", imagePinColour) }, {}, imagePinColour);

			if (appearing)
				drawer.NodeEditorZoomToContent();

			drawer.EndNodeEditor();
		}
	}
}