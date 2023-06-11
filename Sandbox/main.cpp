#include <Core/Logging/Logger.hpp>
#include <Core/Colour.hpp>
#include <Core/Image.hpp>
#include <Core/AsyncData.hpp>
#include <OS/Window.hpp>
#include <OS/Files.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/SceneManager.hpp>
#include <Rendering/VertexData.hpp>
#include <Rendering/GLTFLoader.hpp>
#include <UI/UIManager.hpp>

using namespace Engine;
using namespace Engine::Rendering;
using namespace Engine::Logging;
using namespace Engine::OS;
using namespace Engine::UI;

#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

uint32_t CreateTestMesh(const Renderer& renderer, std::shared_ptr<Image>& image)
{
	return renderer.GetSceneManager().CreateMesh(
		{
			{
				glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(-0.5f, 0.5f, 0.0f),
				glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(-0.5f, 0.5f, -0.5f)
			},
			{
				glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f),
				glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f)
			}
		},
		{
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4
		},
		glm::mat4(1.0f),
		Colour(),
		image);
}

Colour g_clearColour = Colour(0, 0, 0);
Colour g_sunColour = Colour(1, 1, 0.9f);
float g_sunIntensity = 5.0f;
Renderer* g_renderer;
Window* g_window;
bool g_useTAA = true;
bool g_useHDR = false;
AsyncData g_sceneLoad;

std::vector<ScrollingGraphBuffer> g_statGraphBuffers =
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

std::vector<const char*> g_debugModes = { "None", "Albedo", "Normal", "WorldPos", "MetalRoughness", "Cascade Index" };

void DrawUI(const Drawer& drawer)
{
	if (drawer.Begin("UI"))
	{
		if (drawer.BeginTabBar("##uiTabBar"))
		{
			if (drawer.BeginTabItem("Options"))
			{
				int32_t debugMode = static_cast<int32_t>(g_renderer->GetDebugMode());
				if (drawer.ComboBox("Debug Mode", g_debugModes, &debugMode))
				{
					g_renderer->SetDebugMode(debugMode);
				}

				if (drawer.Checkbox("Use Temporal AA", &g_useTAA))
				{
					g_renderer->SetTemporalAAState(g_useTAA);
				}

				bool hdrSupported = g_renderer->IsHDRSupported();
				drawer.BeginDisabled(!hdrSupported);
				if (drawer.Checkbox("Use HDR", &g_useHDR))
				{
					g_renderer->SetHDRState(g_useHDR);
				}
				drawer.EndDisabled();

				if (drawer.Colour3("Clear Colour", g_clearColour))
				{
					g_renderer->SetClearColour(g_clearColour);
				}

				if (drawer.Colour3("Sun Colour", g_sunColour))
				{
					g_renderer->SetSunLightColour(g_sunColour);
				}

				if (drawer.SliderFloat("Sun Intensity", &g_sunIntensity, 0.0f, 20.0f))
				{
					g_renderer->SetSunLightIntensity(g_sunIntensity);
				}

				drawer.EndTabItem();
			}

			if (drawer.BeginTabItem("Statistics"))
			{
				if (drawer.CollapsingHeader("Memory", true))
				{
					const MemoryStats& memoryStats = g_renderer->GetMemoryStats();

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
					drawer.Text("FPS: %.2f", g_renderer->GetUIManager().GetFPS());

					const std::vector<FrameStats>& statsArray = g_renderer->GetRenderStats();
					if (!statsArray.empty())
					{
						for (size_t pass = 0; pass < statsArray.size(); ++pass)
						{
							const FrameStats& stats = statsArray[pass];
							ScrollingGraphBuffer& buffer = g_statGraphBuffers[pass];
							buffer.AddValue(stats.RenderTime);
						}

						glm::vec2 space = drawer.GetContentRegionAvailable();
						drawer.PlotGraphs("Frame Times (ms)", g_statGraphBuffers, space);
					}
				}

				drawer.EndTabItem();
			}

			drawer.EndTabBar();
		}

		drawer.End();
	}
}

void DrawLoadProgress(const Drawer& drawer)
{
	if (g_sceneLoad.State == AsyncState::Completed)
	{
		g_renderer->GetUIManager().UnregisterDrawCallback(DrawLoadProgress);
		g_window->SetCursorVisible(false);
	}

	const ProgressInfo& progress = g_sceneLoad.GetProgress();
	drawer.Progress(progress);

}

int main()
{
	Logger::SetLogOutputLevel(LogLevel::VERBOSE);

	std::unique_ptr<Window> window = Window::Create("Sandbox", glm::uvec2(1280, 720), false);
	std::unique_ptr<Renderer> renderer = Renderer::Create(RendererType::VULKAN, *window, debug);
	g_renderer = renderer.get();
	g_window = window.get();

	if (!renderer || !renderer->Initialise())
	{
		Logger::Error("Failed to initialise renderer.");
		return 1;
	}

	renderer->SetClearColour(g_clearColour);
	renderer->SetSunLightColour(g_sunColour);
	renderer->SetSunLightIntensity(g_sunIntensity);

	renderer->GetUIManager().RegisterDrawCallback(DrawLoadProgress);
	renderer->GetSceneManager().LoadScene("C:/Users/Johan/Desktop/test/Bistro_small.glb", true, g_sceneLoad);

	Camera& camera = renderer->GetCamera();

	float totalTime = 0.0f;
	static auto prevTime = std::chrono::high_resolution_clock::now();

	bool drawUI = false;
	while (!window->IsClosed())
	{
		if (g_sceneLoad.State == AsyncState::Failed)
		{
			return 1;
		}

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
		prevTime = currentTime;
		totalTime += deltaTime;

		if (window->InputState.KeyDown(KeyCode::Escape))
		{
			drawUI = !drawUI;
			window->SetCursorVisible(drawUI);
			if (drawUI)
				renderer->GetUIManager().RegisterDrawCallback(DrawUI);
			else
				renderer->GetUIManager().UnregisterDrawCallback(DrawUI);
		}

		// Rotate sunlight for testing
		glm::vec3 sunDir = glm::vec3(cosf(totalTime), -5.0f, sinf(totalTime));
		//glm::vec3 sunDir = glm::vec3(0, 1, 0) * glm::angleAxis(totalTime, glm::vec3(1, 0, 0));

		renderer->SetSunLightDirection(sunDir);

		if (!drawUI)
		{
			float speed = 10.0f * deltaTime;
			if (window->InputState.KeyPressed(KeyCode::Shift))
			{
				speed *= 2.0f;
			}

			if (window->InputState.KeyPressed(KeyCode::W))
			{
				camera.TranslateLocal(glm::vec3(0.0f, 0.0f, speed));
			}
			if (window->InputState.KeyPressed(KeyCode::A))
			{
				camera.TranslateLocal(glm::vec3(speed, 0.0f, 0.0f));
			}
			if (window->InputState.KeyPressed(KeyCode::S))
			{
				camera.TranslateLocal(glm::vec3(0.0f, 0.0f, -speed));
			}
			if (window->InputState.KeyPressed(KeyCode::D))
			{
				camera.TranslateLocal(glm::vec3(-speed, 0.0f, 0.0f));
			}

			const float mouseSensitivity = 0.005f;
			const glm::vec2& mouseDelta = window->InputState.GetMouseDelta();
			if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
			{
				camera.RotateFPS(mouseDelta.y * mouseSensitivity, mouseDelta.x * mouseSensitivity);
			}
		}

		if (!renderer->Render())
		{
			return 1;
		}

		window->Poll();
	}

	g_sceneLoad.Abort();

	return 0;
}