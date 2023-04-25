#include <Core/Logging/Logger.hpp>
#include <Core/Colour.hpp>
#include <OS/Window.hpp>
#include <OS/Files.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/Mesh.hpp>

using namespace Engine;
using namespace Engine::Rendering;
using namespace Engine::Logging;
using namespace Engine::OS;

#ifndef NDEBUG
const bool debug = true;
#else
const bool debug = false;
#endif

int main()
{
	Logger::SetLogOutputLevel(LogLevel::VERBOSE);

	std::unique_ptr<Window> window = Window::Create("Test", glm::uvec2(1280, 720), false);
	std::unique_ptr<Renderer> renderer = Renderer::Create(RendererType::VULKAN, *window, debug);

	window->RegisterResizeCallback([&renderer](glm::uvec2 size) { renderer->Resize(size); });
	window->RegisterCloseCallback([&renderer]() { renderer->Destroy(); });

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

	std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
	mesh->SetPositions({ glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.5f, -0.5f, 0.0f), glm::vec3(0.5f, 0.5f, 0.0f), glm::vec3(-0.5f, 0.5f, 0.0f)});
	mesh->SetColours({ Colour(1.0f, 1.0f, 0.0f), Colour(0.0f, 1.0f, 0.0f), Colour(0.0f, 0.0f, 1.0f), Colour(1.0f, 0.0f, 0.0f) });
	mesh->SetIndices({ 0, 1, 2, 2, 3, 0 });

	renderer->BeginRenderingMesh(*mesh, shader);

	bool drawingMesh = true;

	while (!window->IsClosed())
	{
		if (window->InputState.KeyDown(KeyCode::V))
		{
			renderer->SetClearColour(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
		}

		if (window->InputState.KeyDown(KeyCode::B))
		{
			drawingMesh = !drawingMesh;
			if (drawingMesh)
				renderer->BeginRenderingMesh(*mesh, shader);
			else
				renderer->StopRenderingMesh(*mesh);
		}

		window->Poll();
	}

	return 0;
}