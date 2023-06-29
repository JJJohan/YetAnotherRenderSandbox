#include <Core/Logging/Logger.hpp>
#include <Core/AsyncData.hpp>
#include <OS/Window.hpp>
#include <Rendering/Renderer.hpp>
#include <Core/SceneManager.hpp>
#include "UI.hpp"
#include "Options.hpp"
#include <UI/Drawer.hpp>
#include <UI/UIManager.hpp>
#include <memory>

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

Renderer* g_renderer;
Window* g_window;
AsyncData g_sceneLoad;
Sandbox::Options g_options;

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

	std::unique_ptr<Sandbox::UI> ui = std::make_unique<Sandbox::UI>(g_options, g_renderer);

	renderer->SetClearColour(g_options.ClearColour);
	renderer->SetSunLightColour(g_options.SunColour);
	renderer->SetSunLightIntensity(g_options.SunIntensity);

	renderer->GetUIManager().RegisterDrawCallback(DrawLoadProgress);
	renderer->GetSceneManager().LoadScene("C:/Users/Johan/Desktop/test/Bistro_small.glb", renderer.get(), true, g_sceneLoad);

	Camera& camera = renderer->GetCamera();

	float totalTime = 0.0f;
	static auto prevTime = std::chrono::high_resolution_clock::now();

	const auto& uiDrawCallback = std::bind(&Sandbox::UI::Draw, ui.get(), std::placeholders::_1);

	bool drawUI = window->InputState.KeyDown(KeyCode::Escape);
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
				renderer->GetUIManager().RegisterDrawCallback(uiDrawCallback);
			else
				renderer->GetUIManager().UnregisterDrawCallback(uiDrawCallback);
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