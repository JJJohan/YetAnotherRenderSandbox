#include <Core/Logging/Logger.hpp>
#include <Core/Colour.hpp>
#include <OS/Window.hpp>
#include <OS/Files.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/MeshManager.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Engine;
using namespace Engine::Rendering;
using namespace Engine::Logging;
using namespace Engine::OS;

#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

uint32_t CreateTestMesh(const Renderer& renderer, const Shader* shader)
{
	return renderer.GetMeshManager()->CreateMesh(shader,
		{ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(-0.5f, 0.5f, 0.0f) },
		{ Colour(1.0f, 1.0f, 0.0f), Colour(0.0f, 1.0f, 0.0f), Colour(0.0f, 0.0f, 1.0f), Colour(1.0f, 0.0f, 0.0f) },
		{ 0, 1, 2, 2, 3, 0 },
		Colour(),
		glm::mat4(1.0f));
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

	uint32_t mesh1 = CreateTestMesh(*renderer, shader);

	uint32_t mesh2 = renderer->GetMeshManager()->CreateMesh(shader,
		{ glm::vec3(-0.5f, -1.0f + -0.5f, 0.0f), glm::vec3(0.5f, -1.0f + -0.5f, 0.0f), glm::vec3(0.5f, -1.0f + 0.5f, 0.0f), glm::vec3(-0.5f, -1.0f + 0.5f, 0.0f) },
		{ Colour(0.0f, 1.0f, 1.0f), Colour(1.0f, 0.0f, 1.0f), Colour(1.0f, 1.0f, 1.0f), Colour(0.0f, 0.0f, 0.0f) },
		{ 0, 1, 2, 2, 3, 0 },
		Colour(),
		glm::mat4(1.0f));

	static auto startTime = std::chrono::high_resolution_clock::now();

	bool drawingMesh = true;

	while (!window->IsClosed())
	{
		if (window->InputState.KeyDown(KeyCode::V))
		{
			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
			glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			renderer->GetMeshManager()->SetTransform(mesh1, model);
		}

		if (window->InputState.KeyDown(KeyCode::B))
		{
			drawingMesh = !drawingMesh;
			if (drawingMesh)
				mesh1 = CreateTestMesh(*renderer, shader);
			else
				renderer->GetMeshManager()->DestroyMesh(mesh1);
		}

		window->Poll();
	}

	return 0;
}