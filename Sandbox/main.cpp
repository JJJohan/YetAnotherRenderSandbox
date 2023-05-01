#include <Core/Logging/Logger.hpp>
#include <Core/Colour.hpp>
#include <Core/Image.hpp>
#include <OS/Window.hpp>
#include <OS/Files.hpp>
#include <Rendering/Renderer.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/MeshManager.hpp>
#include <Rendering/VertexData.hpp>
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
			},
			{
				Colour(), Colour(), Colour(), Colour(),
				Colour(), Colour(), Colour(), Colour()
			}
		},
		{
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4
		},
		Colour(),
		glm::mat4(1.0f),
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

	std::shared_ptr<Image> image = std::make_shared<Image>();
	if (!image->LoadFromFile("C:/Users/Johan/Desktop/texture.jpg"))
	{
		return 1;
	}

	uint32_t mesh1 = CreateTestMesh(*renderer, shader, image);

	uint32_t mesh2 = renderer->GetMeshManager()->CreateMesh(shader,
		{
			{ glm::vec3(-0.5f, -1.0f + -0.5f, 0.0f), glm::vec3(0.5f, -1.0f + -0.5f, 0.0f), glm::vec3(0.5f, -1.0f + 0.5f, 0.0f), glm::vec3(-0.5f, -1.0f + 0.5f, 0.0f) }
		},
		{ 0, 1, 2, 2, 3, 0 },
		Colour(),
		glm::mat4(1.0f),
		nullptr);

	static auto startTime = std::chrono::high_resolution_clock::now();

	bool drawingMesh = true;

	while (!window->IsClosed())
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		renderer->GetMeshManager()->SetTransform(mesh1, model);

		if (window->InputState.KeyDown(KeyCode::B))
		{
			drawingMesh = !drawingMesh;
			if (drawingMesh)
				mesh1 = CreateTestMesh(*renderer, shader, image);
			else
				renderer->GetMeshManager()->DestroyMesh(mesh1);
		}

		window->Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return 0;
}