#include <Core/Logging/Logger.hpp>
#include <Core/Colour.hpp>
#include <Core/Image.hpp>
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
float g_sunIntensity;
uint32_t g_multiSampleCount = 4;
Renderer* g_renderer;
UIManager* g_uiManager;

void DrawUI(const Drawer& drawer)
{
	if (drawer.Begin("Test"))
	{
		drawer.Text("FPS: %.2f", g_uiManager->GetFPS());

		if (drawer.Colour3("Clear Colour", g_clearColour))
		{
			g_renderer->SetClearColour(g_clearColour);
		}

		if (drawer.Colour3("Sun Colour", g_sunColour))
		{
			g_renderer->SetSunLightColour(g_sunColour);
		}
		
		if (drawer.SliderFloat("Sun Intensity", &g_sunIntensity, 0.0f, 10.0f))
		{
			//g_renderer->SetSunLightColour(g_sunColour);
		}

		int32_t multiSampleCount = static_cast<int32_t>(g_multiSampleCount);
		int32_t multiSampleMax = static_cast<int32_t>(g_renderer->GetMaxMultiSampleCount());
		if (drawer.SliderInt("Multisampling", &multiSampleCount, 1, multiSampleMax))
		{
			g_multiSampleCount = static_cast<uint32_t>(multiSampleCount);
			g_renderer->SetMultiSampleCount(g_multiSampleCount);
		}

		drawer.End();
	}
}

int main()
{
	Logger::SetLogOutputLevel(LogLevel::VERBOSE);

	std::unique_ptr<Window> window = Window::Create("Test", glm::uvec2(1280, 720), false);
	std::unique_ptr<Renderer> renderer = Renderer::Create(RendererType::VULKAN, *window, debug);
	g_renderer = renderer.get();

	if (!renderer || !renderer->Initialise())
	{
		Logger::Error("Failed to initialise renderer.");
		return 1;
	}

	g_uiManager = renderer->GetUIManager();

	renderer->SetMultiSampleCount(g_multiSampleCount);
	renderer->SetClearColour(g_clearColour);
	renderer->SetSunLightColour(g_sunColour);

	if (!renderer->GetSceneManager()->LoadScene("C:/Users/Johan/Desktop/test/Bistro_small.glb", true))
	{
		return 1;
	}

	Camera& camera = renderer->GetCamera();

	static auto startTime = std::chrono::high_resolution_clock::now();
	static auto lastRenderTime = std::chrono::high_resolution_clock::now();
	static auto prevTime = startTime;

	window->SetCursorVisible(false);

	bool drawUI = false;
	while (!window->IsClosed())
	{
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

	return 0;
}