#include <Core/Logging/Logger.hpp>
#include <Core/Colour.hpp>
#include <Core/Image.hpp>
#include <OS/Window.hpp>
#include <OS/Files.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/MeshManager.hpp>
#include <Rendering/VertexData.hpp>
#include <Rendering/GLTFLoader.hpp>

using namespace Engine;
using namespace Engine::Rendering;
using namespace Engine::Logging;
using namespace Engine::OS;

#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

uint32_t CreateTestMesh(const Renderer& renderer, const Shader* shader, std::shared_ptr<Image>& image)
{
	return renderer.GetMeshManager()->CreateMesh(shader,
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

int main()
{
	Logger::SetLogOutputLevel(LogLevel::VERBOSE);

	std::unique_ptr<Window> window = Window::Create("Test", glm::uvec2(1280, 720), false);
	std::unique_ptr<Renderer> renderer = Renderer::Create(RendererType::VULKAN, *window, debug);

	renderer->SetClearColour(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	if (!renderer || !renderer->Initialise())
	{
		Logger::Error("Failed to initialise renderer.");
		return 1;
	}

	Shader* shader = renderer->CreateShader("Triangle", {
		{ ShaderProgramType::VERTEX, "Shaders/Triangle_vert.spv" },
		{ ShaderProgramType::FRAGMENT, "Shaders/Triangle_frag.spv" }
		});

	if (shader == nullptr)
	{
		Logger::Error("Shader invalid.");
		return 1;
	}

	std::shared_ptr<Image> image2 = std::make_shared<Image>();
	if (!image2->LoadFromFile("C:/Users/Johan/Desktop/texture.jpg"))
	{
		return 1;
	}

	std::vector<uint32_t> addedMeshes;
	GLTFLoader gltfLoader;
	if (!gltfLoader.LoadGLTF("C:/Users/Johan/Desktop/test/warehouse.glb", renderer->GetMeshManager(), shader, addedMeshes))
	{
		return 1;
	}

	Camera& camera = renderer->GetCamera();

	static auto startTime = std::chrono::high_resolution_clock::now();
	static auto prevTime = startTime;

	uint32_t sampleCount = 1;

	while (!window->IsClosed())
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();

		if (window->InputState.KeyDown(KeyCode::X))
		{
			sampleCount *= 2;
			if (sampleCount > renderer->GetMaxSampleCount())
			{
				sampleCount = 1;
			}

			renderer->SetSampleCount(sampleCount);
			Logger::Info("Set multisample count to {}.", sampleCount);
		}

		if (window->InputState.KeyPressed(KeyCode::W))
		{
			camera.TranslateLocal(glm::vec3(0.0f, 0.0f, 10.0f) * deltaTime);
		}
		if (window->InputState.KeyPressed(KeyCode::A))
		{
			camera.TranslateLocal(glm::vec3(10.0f, 0.0f, 0.0f) * deltaTime);
		}
		if (window->InputState.KeyPressed(KeyCode::S))
		{
			camera.TranslateLocal(glm::vec3(0.0f, 0.0f, -10.0f) * deltaTime);
		}
		if (window->InputState.KeyPressed(KeyCode::D))
		{
			camera.TranslateLocal(glm::vec3(-10.0f, 0.0f, 0.0f) * deltaTime);
		}

		const glm::vec2& mouseDelta = window->InputState.GetMouseDelta();
		if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
		{
			camera.RotateFPS(mouseDelta.y * 0.01f, mouseDelta.x * 0.01f);
		}

		window->Poll();

		prevTime = std::chrono::high_resolution_clock::now();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return 0;
}