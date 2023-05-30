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
	return renderer.GetSceneManager()->CreateMesh(
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
bool g_useHDR = false;
UIManager* g_uiManager;
AsyncData g_sceneLoad;

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
				drawer.Text("FPS: %.2f", g_uiManager->GetFPS());

				std::vector<std::string> passLabels = { "Scene", "Shadow Cascade 1", "Shadow Cascade 2", "Shadow Cascade 3", "Shadow Cascade 4", "Combine", "Total" };

				const std::vector<RenderStatsData>& statsArray = g_renderer->GetRenderStats();
				for (size_t pass = 0; pass < statsArray.size(); ++pass)
				{
					const RenderStatsData& stats = statsArray[pass];
					if (drawer.CollapsingHeader(passLabels[pass].c_str()))
					{
						drawer.Text("Render Time: %.2fms", stats.RenderTime);
						drawer.Text("Input Vertex Count: %i", stats.InputAssemblyVertexCount);
						drawer.Text("Input Primitive Count: %i", stats.InputAssemblyPrimitivesCount);
						drawer.Text("Vertex Shader Invocations: %i", stats.VertexShaderInvocations);
						drawer.Text("Fragment Shader Invocations: %i", stats.FragmentShaderInvocations);
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
		g_uiManager->UnregisterDrawCallback(DrawLoadProgress);
		g_window->SetCursorVisible(false);
	}

	const ProgressInfo& progress = g_sceneLoad.GetProgress();
	drawer.Progress(progress);

}

int main()
{
	Logger::SetLogOutputLevel(LogLevel::VERBOSE);

	std::unique_ptr<Window> window = Window::Create("Test", glm::uvec2(1280, 720), false);
	std::unique_ptr<Renderer> renderer = Renderer::Create(RendererType::VULKAN, *window, debug);
	g_renderer = renderer.get();
	g_window = window.get();

	if (!renderer || !renderer->Initialise())
	{
		Logger::Error("Failed to initialise renderer.");
		return 1;
	}

	g_uiManager = renderer->GetUIManager();

	renderer->SetClearColour(g_clearColour);
	renderer->SetSunLightColour(g_sunColour);
	renderer->SetSunLightIntensity(g_sunIntensity);

	g_uiManager->RegisterDrawCallback(DrawLoadProgress);
	renderer->GetSceneManager()->LoadScene("C:/Users/Johan/Desktop/test/Bistro_small.glb", true, g_sceneLoad);

	Camera& camera = renderer->GetCamera();

	static auto startTime = std::chrono::high_resolution_clock::now();
	static auto lastRenderTime = std::chrono::high_resolution_clock::now();
	static auto prevTime = startTime;

	//window->SetCursorVisible(false);

	bool drawUI = false;
	while (!window->IsClosed())
	{
		if (g_sceneLoad.State == AsyncState::Failed)
		{
			return 1;
		}

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
		float totalTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		float deltaRenderTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastRenderTime).count();

		if (window->InputState.KeyDown(KeyCode::Escape))
		{
			drawUI = !drawUI;
			window->SetCursorVisible(drawUI);
			if (drawUI)
				g_uiManager->RegisterDrawCallback(DrawUI);
			else
				g_uiManager->UnregisterDrawCallback(DrawUI);
		}

		// Rotate sunlight for testing
		glm::vec3 sunDir = glm::vec3(cosf(totalTime), -5.0f, sinf(totalTime));
		//glm::vec3 sunDir = glm::vec3(0, 1, 0) * glm::angleAxis(totalTime, glm::vec3(1, 0, 0));

		renderer->SetSunLightDirection(sunDir);

		if (!drawUI)
		{
			float speed = 100.0f * deltaTime;
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

			const float mouseSensitivity = 0.02f;
			const glm::vec2& mouseDelta = window->InputState.GetMouseDelta();
			if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
			{
				camera.RotateFPS(mouseDelta.y * mouseSensitivity, mouseDelta.x * mouseSensitivity);
			}
		}

		if (deltaRenderTime > (1.0f / 144.0f))
		{
			if (!renderer->Render())
			{
				return 1;
			}

			lastRenderTime = std::chrono::high_resolution_clock::now();
		}

		window->Poll();

		prevTime = std::chrono::high_resolution_clock::now();
	}

	g_sceneLoad.Abort();

	return 0;
}